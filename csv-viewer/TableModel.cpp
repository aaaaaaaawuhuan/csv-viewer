#include "TableModel.h"

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

void TableModel::clear()
{
    beginResetModel();
    m_headers.clear();
    m_dataRows.clear();
    endResetModel();
}