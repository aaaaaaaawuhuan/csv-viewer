#include "CsvReader.h"
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QElapsedTimer>
#include "csv.hpp"

CsvReader::CsvReader(QObject *parent)
    : QObject(parent)
{
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
    
    // 使用Qt打开文件（支持中文路径），然后将内容传递给第三方库
    try {
        // 转换QString路径为UTF-8编码的std::string
        std::string utf8FilePath = filePath.toUtf8().constData();
        
        // 使用第三方库的CSVReader直接打开文件
        using namespace csv;
        
        // 计时：库读取文件
        QElapsedTimer libraryReadTimer;
        libraryReadTimer.start();
        CSVReader reader(utf8FilePath);
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
