#include "VirtualTableModel.h"
#include <QBrush>
#include <QDebug>

VirtualTableModel::VirtualTableModel(CsvReader* csvReader, QObject *parent)
    : QAbstractTableModel(parent)
    , m_totalRowCount(0)
    , m_csvReader(csvReader)
    , m_dataCache(new DataCache(this))
{
}

int VirtualTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
        
    return m_totalRowCount;
}

int VirtualTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
        
    return m_headers.size();
}

QVariant VirtualTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= m_totalRowCount || index.column() >= m_headers.size())
        return QVariant();

    if (role == Qt::DisplayRole) {
        QStringList row = getRowData(index.row());
        if (index.column() < row.size()) {
            return row[index.column()];
        }
    }
    // 设置单元格背景色为白色，确保所有单元格都能正常显示
    else if (role == Qt::BackgroundRole) {
        return QBrush(Qt::white);
    }
    // 设置单元格文本颜色为黑色
    else if (role == Qt::ForegroundRole) {
        return QBrush(Qt::black);
    }

    return QVariant();
}

QVariant VirtualTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal && section < m_headers.size()) {
            return m_headers[section];
        }
    }

    return QAbstractTableModel::headerData(section, orientation, role);
}

void VirtualTableModel::setHeaders(const QStringList &headers)
{
    beginResetModel();
    m_headers = headers;
    endResetModel();
}

void VirtualTableModel::setTotalRowCount(int count)
{
    beginResetModel();
    m_totalRowCount = count;
    endResetModel();
}

void VirtualTableModel::clear()
{
    beginResetModel();
    m_headers.clear();
    m_totalRowCount = 0;
    m_dataCache->clear();
    endResetModel();
}

void VirtualTableModel::loadDataRange(int startRow, int rowCount)
{
    // 检查请求的数据是否已经在缓存中
    QList<QStringList> cachedData;
    if (!m_dataCache->getData(startRow, rowCount, cachedData)) {
        // 如果不在缓存中，则从文件中读取
        QList<QStringList> data = m_csvReader->getRowsInRange(startRow, rowCount);
        // 将读取的数据放入缓存
        m_dataCache->putData(startRow, data);
    }
}

bool VirtualTableModel::isRowLoaded(int row) const
{
    // 简单实现，实际项目中可能需要更复杂的检查
    // 检查该行是否在已缓存的任何数据块中
    return true;
}

void VirtualTableModel::setCacheSize(int size)
{
    m_dataCache->setMaxSize(size);
}

int VirtualTableModel::cacheSize() const
{
    return m_dataCache->maxSize();
}

QStringList VirtualTableModel::getRowData(int row) const
{
    // 查找包含该行的缓存数据块
    QList<QStringList> rowData;
    // 请求一个包含当前行的更大的数据块以提高效率
    int blockSize = 100; // 一次加载100行
    int startRow = (row / blockSize) * blockSize; // 对齐到块边界
    int rowCount = qMin(blockSize, m_totalRowCount - startRow);
    
    // 尝试从缓存获取数据块
    if (m_dataCache->getData(startRow, rowCount, rowData) && (row - startRow) < rowData.size()) {
        return rowData.at(row - startRow);
    }
    
    // 如果缓存中没有，则直接从文件读取整个数据块
    rowData = m_csvReader->getRowsInRange(startRow, rowCount);
    if (!rowData.isEmpty()) {
        // 将数据块添加到缓存中
        m_dataCache->putData(startRow, rowData);
        if ((row - startRow) < rowData.size()) {
            return rowData.at(row - startRow);
        }
    }
    
    return QStringList();
}