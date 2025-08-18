void CsvReader::buildRowOffsetIndex()
{
    if (m_rowOffsetIndexBuilt || m_filePath.isEmpty()) {
        return;
    }
    
    qDebug() << "Building row offset index for file:" << m_filePath;
    
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open file for building index:" << m_filePath;
        return;
    }
    
    // 清空之前的索引并添加起始位置0（第0行）
    m_rowOffsets.clear();
    m_rowOffsets.push_back(0);
    
    // 使用更高效的方式读取行偏移量
    const qint64 chunkSize = 1024 * 1024; // 1MB块大小
    qint64 position = 0;
    QByteArray buffer;
    
    while (!file.atEnd()) {
        buffer = file.read(chunkSize);
        if (buffer.isEmpty()) {
            break;
        }
        
        // 在缓冲区中查找换行符
        for (int i = 0; i < buffer.size(); ++i) {
            if (buffer[i] == '\n') {
                m_rowOffsets.push_back(position + i + 1);
            }
        }
        
        position += buffer.size();
    }
    
    m_rowOffsetIndexBuilt = true;
    qDebug() << "Row offset index built with" << m_rowOffsets.size() << "entries";
    file.close();
}

bool CsvReader::isRowOffsetIndexBuilt() const
{
    return m_rowOffsetIndexBuilt;
}

#include "CsvReader.h"
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QElapsedTimer>
#include <QStringConverter>
#include <sstream>
#include <algorithm>
#include <vector>
#include "csv.hpp"

CsvReader::CsvReader(QObject *parent)
    : QObject(parent)
    , m_encoding(GBK) // 默认使用GBK编码
    , m_totalRowCount(0)
    , m_hasMoreData(false)
    , m_lastLoadedRow(-1)
    , m_totalRowsEstimated(false)
    , m_estimatedTotalRows(0)
    , m_rowOffsetIndexBuilt(false)
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
    m_filePath = filePath; // 保存文件路径
    m_rowOffsetIndexBuilt = false; // 重置行偏移索引标记
    
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
    
    try {
        // 快速估算总行数（不完全读取文件）
        m_totalRowCount = estimateTotalRows();
        qDebug() << "Estimated total rows:" << m_totalRowCount;
        
        // 使用Qt打开文件（支持中文路径和不同编码），然后将内容传递给第三方库
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_lastError = QString("Failed to open file: %1, error: %2").arg(filePath).arg(file.errorString());
            qDebug() << m_lastError;
            return false;
        }
        
        // 只读取前几行内容，而不是整个文件（避免大文件占用过多内存）
        QByteArray fileContent;
        const int MAX_PREVIEW_LINES = 1000; // 只读取前1000行
        int lineCount = 0;
        while (!file.atEnd() && lineCount < MAX_PREVIEW_LINES) {
            fileContent.append(file.readLine());
            lineCount++;
        }
        file.close();
        
        // 使用统一的编码处理函数
        QString content = decodeFileContent(fileContent);
        
        // 将QString转换为UTF-8编码的std::string
        std::string utf8Content = content.toUtf8().constData();
        
        // 将内容放入std::istringstream中，以便csv库读取
        std::istringstream csvStream(utf8Content);
        
        // 使用第三方库的CSVReader从流中读取
        using namespace csv;
        CSVReader reader(csvStream);
        
        // 获取表头
        auto col_names = reader.get_col_names();
        qDebug() << "Number of columns detected:" << col_names.size();
        m_headers.clear();
        m_headers.reserve(col_names.size());
        for (const auto& name : col_names) {
            m_headers.append(QString::fromStdString(name));
        }
        
        // 读取初始数据行（界面显示最大行数+余量）
        m_dataRows.clear();
        // 初始加载行数：设置为较小值以提高启动速度
        const int INITIAL_ROWS = 1000; // 可根据需要调整
        m_dataRows.reserve(std::min(INITIAL_ROWS, (int)m_totalRowCount));
        int rowCount = 0;
        
        // 流式处理，仅加载需要的数据
        for (auto& row : reader) {
            // 超过初始行数后停止加载
            if (rowCount >= INITIAL_ROWS) {
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
        
        // 记录总行数（估算值）
        // 不再加载所有数据，而是使用虚拟滚动和按需加载
        m_lastLoadedRow = rowCount - 1;
        
        // 标记为有更多数据（如果总行数大于初始加载行数）
        m_hasMoreData = (m_totalRowCount > INITIAL_ROWS);
        
        qDebug() << "Initially loaded rows:" << rowCount << ", Total estimated rows:" << m_totalRowCount;
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

// 大文件优化相关方法实现
qint64 CsvReader::estimateTotalRows()
{
    // 如果已经计算过，直接返回结果
    if (m_totalRowsEstimated) {
        return m_estimatedTotalRows;
    }
    
    if (m_filePath.isEmpty()) {
        return 0;
    }
    
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open file for reading:" << m_filePath;
        return 0;
    }
    
    qint64 fileSize = file.size();
    // 对于小文件（小于10MB），直接计算行数
    if (fileSize < 10 * 1024 * 1024) {
        qint64 lineCount = 0;
        while (!file.atEnd()) {
            file.readLine();
            lineCount++;
        }
        m_estimatedTotalRows = lineCount;
        m_totalRowsEstimated = true;
        return lineCount;
    }
    
    // 对于大文件，使用采样法估算行数
    const int sampleSize = 100 * 1024; // 100KB样本
    const int sampleCount = 3; // 3个样本点（前、中、后）
    
    qint64 totalLineCount = 0;
    qint64 sampledBytes = 0;
    
    for (int i = 0; i < sampleCount; i++) {
        qint64 position = 0;
        if (i == 0) {
            // 第一个样本在文件开头
            position = 0;
        } else if (i == 1) {
            // 第二个样本在文件中间
            position = fileSize / 2;
        } else {
            // 第三个样本在文件末尾
            position = std::max(fileSize - sampleSize, (qint64)0);
        }
        
        file.seek(position);
        QByteArray sampleData = file.read(sampleSize);
        sampledBytes += sampleData.size();
        
        // 计算样本中的行数
        qint64 lineCountInSample = 0;
        for (int j = 0; j < sampleData.size(); j++) {
            if (sampleData[j] == '\n') {
                lineCountInSample++;
            }
        }
        
        totalLineCount += lineCountInSample;
    }
    
    // 根据采样比例估算总行数
    if (sampledBytes > 0) {
        double ratio = (double)totalLineCount / sampledBytes;
        m_estimatedTotalRows = static_cast<qint64>(ratio * fileSize);
        m_totalRowsEstimated = true;
        return m_estimatedTotalRows;
    }
    
    m_estimatedTotalRows = 0;
    m_totalRowsEstimated = true;
    return 0;
}

QString CsvReader::decodeFileContent(const QByteArray& fileContent) const
{
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
    
    return content;
}

QList<QStringList> CsvReader::getRowsInRange(int startRow, int rowCount)
{
    QList<QStringList> result;
    
    if (m_filePath.isEmpty() || rowCount <= 0) {
        return result;
    }
    
    try {
        QFile file(m_filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            m_lastError = QString("Failed to open file: %1, error: %2").arg(m_filePath).arg(file.errorString());
            qDebug() << m_lastError;
            return result;
        }
        
        // 如果是第一次调用此方法，建立行偏移索引
        if (!isRowOffsetIndexBuilt()) {
            buildRowOffsetIndex();
        }
        
        // 如果索引建立成功，则使用索引优化读取
        if (isRowOffsetIndexBuilt() && startRow < (int)m_rowOffsets.size()) {
            // 定位到起始行
            qint64 startPosition = m_rowOffsets[startRow];
            file.seek(startPosition);
            
            // 计算结束位置（如果可能的话）
            qint64 endPosition = file.size(); // 默认为文件末尾
            if (startRow + rowCount < (int)m_rowOffsets.size()) {
                endPosition = m_rowOffsets[startRow + rowCount];
            }
            
            // 读取指定范围的内容
            qint64 bytesToRead = endPosition - startPosition;
            QByteArray partialContent = file.read(bytesToRead);
            
            // 解码内容
            QString content = decodeFileContent(partialContent);
            
            // 解析CSV数据
            std::string utf8Content = content.toUtf8().constData();
            std::istringstream csvStream(utf8Content);
            
            using namespace csv;
            CSVReader reader(csvStream);
            
            // 读取所有解析出来的行
            int rowsRead = 0;
            for (auto& row : reader) {
                if (rowsRead >= rowCount) {
                    break;
                }
                
                QStringList qRow;
                qRow.reserve(row.size());
                
                // 处理数据行
                for (size_t i = 0; i < row.size(); i++) {
                    qRow.append(QString::fromStdString(row[i].get<std::string>()));
                }
                
                result.append(qRow);
                rowsRead++;
            }
        } else {
            // 回退到原始实现（逐行跳过）
            QString content = decodeFileContent(file.readAll());
            file.close();
            
            std::string utf8Content = content.toUtf8().constData();
            std::istringstream csvStream(utf8Content);
            
            using namespace csv;
            CSVReader reader(csvStream);
            
            // 跳过前面的行
            int rowsSkipped = 0;
            for (auto& row : reader) {
                if (rowsSkipped >= startRow) {
                    break;
                }
                rowsSkipped++;
            }
            
            // 读取指定数量的行
            int rowsRead = 0;
            for (auto& row : reader) {
                if (rowsRead >= rowCount) {
                    break;
                }
                
                QStringList qRow;
                qRow.reserve(row.size());
                
                for (size_t i = 0; i < row.size(); i++) {
                    qRow.append(QString::fromStdString(row[i].get<std::string>()));
                }
                
                result.append(qRow);
                rowsRead++;
            }
        }
        
        return result;
    } catch (const std::exception &e) {
        m_lastError = QString("Error reading rows in range: %1").arg(e.what());
        qDebug() << m_lastError;
        return result;
    } catch (...) {
        m_lastError = "Unknown error occurred while reading rows in range";
        qDebug() << m_lastError;
        return result;
    }
}
