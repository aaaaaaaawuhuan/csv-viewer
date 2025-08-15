#include "CsvReader.h"
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QElapsedTimer>
#include <QStringConverter>
#include <sstream>
#include "csv.hpp"

CsvReader::CsvReader(QObject *parent)
    : QObject(parent)
    , m_encoding(UTF8) // 默认使用UTF-8编码
{
}

void CsvReader::setEncoding(Encoding encoding)
{
    m_encoding = encoding;
}

CsvReader::Encoding CsvReader::getEncoding() const
{
    return m_encoding;
}

bool CsvReader::loadFile(const QString &filePath)
{
    // 清空之前的数据
    m_headers.clear();
    m_dataRows.clear();
    m_lastError.clear();
    
    // 检查文件是否存在和可读
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        m_lastError = QString("File does not exist: %1").arg(filePath);
        qDebug() << m_lastError;
        return false;
    }
    
    if (!fileInfo.isFile()) {
        m_lastError = QString("Path is not a file: %1").arg(filePath);
        qDebug() << m_lastError;
        return false;
    }
    
    if (!fileInfo.isReadable()) {
        m_lastError = QString("File is not readable (permission denied): %1").arg(filePath);
        qDebug() << m_lastError;
        return false;
    }
    
    // 检查文件大小
    qint64 fileSize = fileInfo.size();
    if (fileSize == 0) {
        m_lastError = QString("File is empty: %1").arg(filePath);
        qDebug() << m_lastError;
        return false;
    }
    
    qDebug() << "File exists and is readable:" << filePath << "Size:" << fileSize << "bytes";
    
    // 使用Qt打开文件（支持中文路径和不同编码），然后将内容传递给第三方库
    try {
        // 计时：库读取文件
        QElapsedTimer libraryReadTimer;
        libraryReadTimer.start();
        
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_lastError = QString("Failed to open file: %1, error: %2").arg(filePath).arg(file.errorString());
            qDebug() << m_lastError;
            return false;
        }
        
        // 读取文件内容
        QByteArray fileContent = file.readAll();
        file.close();
        
        // 初始化内容
        QString content;
        
        // 根据编码设置选择解码器
        if (m_encoding == UTF8) {
            content = QString::fromUtf8(fileContent);
        } else if (m_encoding == GBK) {
            // 在Qt6中使用fromLocal8Bit处理GBK编码
            content = QString::fromLocal8Bit(fileContent);
        } else { // AutoDetect
            // 读取文件头进行简单编码检测
            if (fileContent.startsWith("\xEF\xBB\xBF")) {
                // 移除BOM并使用UTF-8
                content = QString::fromUtf8(fileContent.mid(3));
                qDebug() << "Auto-detected encoding: UTF-8 (with BOM)";
            } else {
                // 使用简单策略检测编码：先尝试UTF-8
                QString utf8Content = QString::fromUtf8(fileContent);
                // 检查UTF-8解码是否成功（基本策略：没有乱码标记）
                bool isUtf8 = true;
                for (const QChar &c : utf8Content) {
                    // 检查是否有替换字符（通常表示解码失败）
                    if (c.unicode() == 0xFFFD) {
                        isUtf8 = false;
                        break;
                    }
                }
                
                if (isUtf8) {
                    content = utf8Content;
                    qDebug() << "Auto-detected encoding: UTF-8";
                } else {
                    // 否则使用local8Bit（对于Windows系统，通常支持GBK）
                    content = QString::fromLocal8Bit(fileContent);
                    qDebug() << "Auto-detected encoding: Local (GBK compatible)";
                }
            }
        }
        
        // 检查转换是否成功
        if (content.isEmpty() && !fileContent.isEmpty()) {
            qWarning() << "Content conversion failed, trying UTF-8 as fallback";
            content = QString::fromUtf8(fileContent);
        }
        
        // 将QString转换为UTF-8编码的std::string
        std::string utf8Content = content.toUtf8().constData();
        
        // 将内容放入std::istringstream中，以便csv库读取
        std::istringstream csvStream(utf8Content);
        
        // 使用第三方库的CSVReader从流中读取
        using namespace csv;
        CSVReader reader(csvStream);
        
        qint64 libraryReadTime = libraryReadTimer.elapsed();
        qDebug() << "Library read time:" << libraryReadTime << "ms";

            // 计时：添加表头
            QElapsedTimer headersTimer;
            headersTimer.start();
            
            // 获取表头
            auto col_names = reader.get_col_names();
            qDebug() << "Number of columns detected:" << col_names.size();
            m_headers.clear();
            for (const auto& name : col_names) {
                m_headers.append(QString::fromStdString(name));
            }
            
            qint64 headersTime = headersTimer.elapsed();
            qDebug() << "Headers processing time:" << headersTime << "ms";

            // 计时：读取数据行
            QElapsedTimer rowsTimer;
            rowsTimer.start();
            
            // 读取数据行
            m_dataRows.clear();
            int rowCount = 0;
            for (auto& row : reader) {
                QStringList qRow;
                for (size_t i = 0; i < row.size(); i++) {
                    qRow.append(QString::fromStdString(row[i].get<std::string>()));
                }
                m_dataRows.append(qRow);
                rowCount++;
            }
            
            qint64 rowsTime = rowsTimer.elapsed();
            qDebug() << "Rows processing time:" << rowsTime << "ms" << "(" << rowCount << " rows)";

            qDebug() << "Total rows read:" << rowCount;
            return true;
    } catch (const std::exception &e) {
        m_lastError = QString("Error parsing CSV file: %1").arg(e.what());
        qDebug() << m_lastError;
        return false;
    } catch (...) {
        m_lastError = "Unknown error occurred while parsing CSV file";
        qDebug() << m_lastError;
        return false;
    }
}



QStringList CsvReader::getHeaders() const
{
    return m_headers;
}

int CsvReader::getRowCount() const
{
    return m_dataRows.size();
}

QStringList CsvReader::getRow(int index) const
{
    if (index >= 0 && index < m_dataRows.size()) {
        return m_dataRows.at(index);
    }
    return QStringList();
}

QList<QStringList> CsvReader::getAllRows() const
{
    return m_dataRows;
}

QList<QStringList> CsvReader::getRowsRange(int startIndex, int count) const
{
    QList<QStringList> result;
    
    // 验证索引范围
    if (startIndex < 0 || count <= 0 || startIndex >= m_dataRows.size()) {
        return result;
    }
    
    // 计算实际要获取的行数
    int actualCount = qMin(count, m_dataRows.size() - startIndex);
    
    // 获取指定范围的数据行
    for (int i = 0; i < actualCount; ++i) {
        result.append(m_dataRows.at(startIndex + i));
    }
    
    return result;
}

QString CsvReader::getLastError() const
{
    return m_lastError;
}
