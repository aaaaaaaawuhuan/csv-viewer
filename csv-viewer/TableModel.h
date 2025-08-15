#ifndef TABLEMODEL_H
#define TABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QStringList>
#include <QVector>

class TableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit TableModel(QObject *parent = nullptr);

    // 基本的模型接口实现
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // 设置数据
    void setHeaders(const QStringList &headers);
    void addRow(const QStringList &row);
    void addRows(const QList<QStringList> &rows); // 批量添加数据行，优化性能
    void clear();

private:
    QStringList m_headers;
    QList<QStringList> m_dataRows;
};

#endif // TABLEMODEL_H