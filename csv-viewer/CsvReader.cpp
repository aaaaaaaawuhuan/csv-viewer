#include "CsvReader.h"
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
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
    
    // 使用vincentlaucsb的CSV解析库解析文件
    try {
        return parseCsvFile(filePath.toStdString());
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

bool CsvReader::parseCsvFile(const std::string &filename)
{
    try {
        // 使用vincentlaucsb的csv-parser库
        using namespace csv;
        
        qDebug() << "Attempting to parse file with csv-parser library:" << QString::fromStdString(filename);
        
        // 创建CSV读取器
        CSVReader reader(filename);
        
        // 获取表头
        auto col_names = reader.get_col_names();
        qDebug() << "Number of columns detected:" << col_names.size();
        
        for (const auto& name : col_names) {
            m_headers.append(QString::fromStdString(name));
            qDebug() << "Column:" << QString::fromStdString(name);
        }
        
        // 读取数据行
        m_dataRows.clear();
        int rowCount = 0;
        for (auto& row : reader) {  // 注意：这里使用非const引用
            QStringList qRow;
            for (size_t i = 0; i < row.size(); i++) {
                // 使用索引访问字段并获取其值
                qRow.append(QString::fromStdString(row[i].get<std::string>()));
            }
            m_dataRows.append(qRow);
            rowCount++;
            
            // 限制输出调试信息的数量
            if (rowCount <= 5) {
                qDebug() << "Row" << rowCount << ":" << qRow;
            } else if (rowCount == 6) {
                qDebug() << "... (additional rows omitted for brevity)";
            }
        }
        
        qDebug() << "Total rows read:" << rowCount;
        return true;
    } catch (const std::exception &e) {
        m_lastError = QString("Error parsing CSV file: %1").arg(e.what());
        qDebug() << m_lastError;
        return false;
    } catch (...) {
        m_lastError = "Unknown error occurred in parseCsvFile";
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

QString CsvReader::getLastError() const
{
    return m_lastError;
}