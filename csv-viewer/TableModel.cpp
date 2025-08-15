#include "TableModel.h"
#include <QBrush>

TableModel::TableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int TableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
        
    return m_dataRows.size();
}

int TableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
        
    return m_headers.size();
}

QVariant TableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::DisplayRole) {
        if (index.row() < m_dataRows.size() && index.column() < m_dataRows[index.row()].size()) {
            return m_dataRows[index.row()][index.column()];
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

QVariant TableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal && section < m_headers.size()) {
            return m_headers[section];
        }
    }

    return QAbstractTableModel::headerData(section, orientation, role);
}

void TableModel::setHeaders(const QStringList &headers)
{
    beginResetModel();
    m_headers = headers;
    endResetModel();
}

void TableModel::addRow(const QStringList &row)
{
    beginInsertRows(QModelIndex(), m_dataRows.size(), m_dataRows.size());
    m_dataRows.append(row);
    endInsertRows();
}

// 批量添加数据行 - 优化性能
void TableModel::addRows(const QList<QStringList> &rows)
{
    if (rows.isEmpty())
        return;
        
    beginInsertRows(QModelIndex(), m_dataRows.size(), m_dataRows.size() + rows.size() - 1);
    m_dataRows.append(rows);
    endInsertRows();
}

void TableModel::clear()
{
    beginResetModel();
    m_headers.clear();
    m_dataRows.clear();
    endResetModel();
}