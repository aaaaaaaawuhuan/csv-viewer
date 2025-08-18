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
    
    // 支持的文件编码枚举
    enum Encoding {
        UTF8,    // UTF-8编码
        GBK,     // 中文GBK编码
        AutoDetect  // 自动检测编码
    };
    
    // 读取CSV文件
    bool loadFile(const QString &filePath);
    
    // 设置文件编码
    void setEncoding(Encoding encoding);
    
    // 获取当前设置的编码
    Encoding getEncoding() const;
    
    // 获取表头
    QStringList getHeaders() const;
    
    // 获取数据行数
    int getRowCount() const;
    
    // 获取指定行的数据
    QStringList getRow(int index) const;
    
    // 获取所有数据行 - 用于批量处理
    QList<QStringList> getAllRows() const;
    
    // 获取指定范围的数据行 - 用于限制初始加载行数
    QList<QStringList> getRowsRange(int startIndex, int count) const;
    
    // 获取错误信息
    QString getLastError() const;
    
    // 延迟加载相关方法
    bool hasMoreData() const; // 是否有更多数据未加载
    int getEstimatedTotalRows() const; // 获取估计的总行数
    bool loadMoreRows(int count); // 加载更多数据行
    int getLastLoadedRowIndex() const; // 获取最后加载的行索引
    
    // 大文件优化相关方法
    qint64 estimateTotalRows(); // 快速估算文件总行数
    QList<QStringList> getRowsInRange(int startRow, int rowCount); // 获取指定范围的数据
    
    // 添加行偏移索引相关方法
    void buildRowOffsetIndex();
    bool isRowOffsetIndexBuilt() const;

private:
    // CSV数据存储
    QStringList m_headers;
    QList<QStringList> m_dataRows;
    QString m_lastError;
    Encoding m_encoding; // 当前设置的编码
    QString m_filePath; // 保存文件路径用于后续读取
    
    // 延迟加载相关成员变量
    int m_totalRowCount; // 估计的总行数
    bool m_hasMoreData; // 是否还有更多数据未加载
    int m_lastLoadedRow; // 最后加载的行索引
    QString m_fileContentBackup; // 保存文件内容用于后续加载

    // 性能优化相关成员变量
    bool m_totalRowsEstimated;
    qint64 m_estimatedTotalRows;
    
    // 行偏移索引相关成员变量
    std::vector<qint64> m_rowOffsets;
    bool m_rowOffsetIndexBuilt;
};

#endif // CSVREADER_H