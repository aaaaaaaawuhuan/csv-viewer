#ifndef CSVREADER_H
#define CSVREADER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QStringList>

// 包含vincentlaucsb的CSV解析库
#include "csv.hpp"

class CsvReader : public QObject
{
    Q_OBJECT

public:
    explicit CsvReader(QObject *parent = nullptr);
    
    // 读取CSV文件
    bool loadFile(const QString &filePath);
    
    // 获取表头
    QStringList getHeaders() const;
    
    // 获取数据行数
    int getRowCount() const;
    
    // 获取指定行的数据
    QStringList getRow(int index) const;
    
    // 获取错误信息
    QString getLastError() const;

private:
    // CSV数据存储
    QStringList m_headers;
    QList<QStringList> m_dataRows;
    QString m_lastError;
    
    // 解析CSV文件
    bool parseCsvFile(const std::string &filename);
};

#endif // CSVREADER_H