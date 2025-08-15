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
    , m_encoding(GBK) // 默认使用UTF-8编码
    , m_totalRowCount(0)
    , m_hasMoreData(false)
    , m_lastLoadedRow(-1)
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
            
            // 优化1: 预分配容器大小以减少重分配
            m_dataRows.clear();
            m_dataRows.reserve(25000); // 根据预估的行数预分配
            int rowCount = 0;
            
            // 优化2: 减少内存重分配
            // 优化3: 流式处理，仅加载需要的数据
            const int MAX_INITIAL_ROWS = 10000; // 初始加载的最大行数
            for (auto& row : reader) {
                // 超过初始行数后停止加载
                if (rowCount >= MAX_INITIAL_ROWS) {
                    break;
                }
                
                QStringList qRow;
                qRow.reserve(row.size()); // 预分配每行列数
                
                // 处理数据行
                for (size_t i = 0; i < row.size(); i++) {
                    qRow.append(QString::fromStdString(row[i].get<std::string>()));
                }
                
                m_dataRows.append(qRow);
                rowCount++;
            }
            
            qint64 rowsTime = rowsTimer.elapsed();
            qDebug() << "Rows processing time:" << rowsTime << "ms" << "(" << rowCount << " rows loaded initially)";
            qDebug() << "Memory usage: ~" << (m_dataRows.capacity() * m_headers.size() * 20) / (1024*1024) << "MB (estimated)";
            
            // 记录总行数，但不加载所有数据
            // 这是一个估算值，实际行数可能需要完整读取文件才能确定
            // 对于大文件，可以考虑保存文件内容以便后续分页加载
            m_totalRowCount = rowCount;
            if (rowCount >= MAX_INITIAL_ROWS) {
                // 如果文件还有更多行，标记为需要延迟加载
                m_hasMoreData = true;
                m_lastLoadedRow = rowCount - 1;
                m_fileContentBackup = content; // 保存文件内容用于后续加载
                qDebug() << "File may have more data, enabled lazy loading.";
            } else {
                m_hasMoreData = false;
                m_lastLoadedRow = rowCount - 1;
                m_totalRowCount = rowCount;
            }
            
            qDebug() << "Initially loaded rows:" << rowCount;
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

// 延迟加载相关方法实现
bool CsvReader::hasMoreData() const
{
    return m_hasMoreData;
}

int CsvReader::getEstimatedTotalRows() const
{
    return m_totalRowCount;
}

int CsvReader::getLastLoadedRowIndex() const
{
    return m_lastLoadedRow;
}

bool CsvReader::loadMoreRows(int count)
{
    if (!m_hasMoreData || m_fileContentBackup.isEmpty()) {
        return false;
    }
    
    try {
        QElapsedTimer loadTimer;
        loadTimer.start();
        
        // 将QString转换为UTF-8编码的std::string
        std::string utf8Content = m_fileContentBackup.toUtf8().constData();
        
        // 将内容放入std::istringstream中，以便csv库读取
        std::istringstream csvStream(utf8Content);
        
        // 使用第三方库的CSVReader从流中读取
        using namespace csv;
        CSVReader reader(csvStream);
        
        // 跳过已经加载的行
        int rowsSkipped = 0;
        for (auto& row : reader) {
            if (rowsSkipped > m_lastLoadedRow) {
                break;
            }
            rowsSkipped++;
        }
        
        // 加载指定数量的新行
        int newRowsLoaded = 0;
        for (auto& row : reader) {
            if (newRowsLoaded >= count) {
                break;
            }
            
            QStringList qRow;
            qRow.reserve(row.size()); // 预分配每行列数
            
            // 处理数据行
            for (size_t i = 0; i < row.size(); i++) {
                qRow.append(QString::fromStdString(row[i].get<std::string>()));
            }
            
            m_dataRows.append(qRow);
            newRowsLoaded++;
            m_lastLoadedRow++;
        }
        
        // 检查是否还有更多数据
        if (newRowsLoaded < count) {
            m_hasMoreData = false;
            m_totalRowCount = m_lastLoadedRow + 1;
        }
        
        qDebug() << "Loaded" << newRowsLoaded << "more rows in" << loadTimer.elapsed() << "ms";
        qDebug() << "Total rows loaded now:" << m_dataRows.size();
        qDebug() << "Memory usage: ~" << (m_dataRows.capacity() * m_headers.size() * 20) / (1024*1024) << "MB (estimated)";
        
        return newRowsLoaded > 0;
    } catch (const std::exception &e) {
        m_lastError = QString("Error loading more rows: %1").arg(e.what());
        qDebug() << m_lastError;
        return false;
    } catch (...) {
        m_lastError = "Unknown error occurred while loading more rows";
        qDebug() << m_lastError;
        return false;
    }
}
