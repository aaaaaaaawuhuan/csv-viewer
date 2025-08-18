#ifndef VIRTUALTABLEMODEL_H
#define VIRTUALTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QStringList>
#include <QSharedPointer>
#include "DataCache.h"
#include "CsvReader.h"

class VirtualTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit VirtualTableModel(CsvReader* csvReader, QObject *parent = nullptr);
    
    // 基本的模型接口实现
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    // 设置数据
    void setHeaders(const QStringList &headers);
    void setTotalRowCount(int count);
    void clear();
    
    // 数据加载相关
    void loadDataRange(int startRow, int rowCount);
    bool isRowLoaded(int row) const;
    
    // 缓存管理
    void setCacheSize(int size);
    int cacheSize() const;

private:
    QStringList m_headers;
    int m_totalRowCount;
    CsvReader* m_csvReader;
    QSharedPointer<DataCache> m_dataCache;
    
    // 获取指定行的数据
    QStringList getRowData(int row) const;
};

#endif // VIRTUALTABLEMODEL_H