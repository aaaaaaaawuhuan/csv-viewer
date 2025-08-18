#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QMessageBox>
#include <QAction>
#include <QActionGroup>
#include <QTableView>
#include <QCheckBox>
#include <QVector>
#include <QPair>
#include <QScrollBar>

#include "CsvReader.h"
#include "TableModel.h"
#include "VirtualTableModel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 打开文件槽函数
    void openFile();
    
    // 加载更多行数据
    void loadMoreRows();
    
    // 处理筛选按钮点击
    void applyFilter();
    
    // 全选所有列
    void selectAllColumns();
    
    // 清空所有列选择
    void clearAllColumns();
    
    // 切换筛选面板的显示/隐藏状态
    void toggleFilterPanel(bool visible);
    
    // 根据输入过滤复选框显示
    void filterCheckboxes(const QString &text);
    
    // 处理自定义垂直滚动条的值变化
    void handleVerticalScroll(int value);

private:
    // 加载CSV文件
    void loadCsvFile(const QString &filePath);
    
    // 显示CSV数据
    void displayCsvData(bool loadAll = false);
    
    // 创建编码选择菜单
    void createEncodingMenu();
    
    // 在编码变更时重新加载当前文件（如果有）
    void reloadCurrentFileIfNeeded();
    
    // 设置筛选面板
    void setupFilterPanel(const QStringList &headers);
    
    // 更新表格显示以反映筛选结果
    void updateFilteredColumns();
    
    // 重置筛选面板
    void resetFilterPanel();
    
    // 搜索输入框
    QLineEdit *m_searchLineEdit = nullptr;

    Ui::MainWindow *ui;
    CsvReader *m_csvReader;
    TableModel *m_tableModel;
    VirtualTableModel *m_virtualTableModel;
    QScrollBar *m_verticalScrollBar; // 自定义垂直滚动条
    
    // 性能优化相关成员
    const int DEFAULT_ROWS_LIMIT = 5000; // 默认初始加载行数限制
    int m_currentLoadedRows = 0; // 当前已加载的行数
    int m_totalRows = 0; // 文件总行数
    int m_visibleRows = 100; // 可见行数
    
    // 当前打开的文件路径，用于编码变更时重新加载
    QString m_currentFilePath;
    
    // 筛选相关成员
    QVector<QPair<QCheckBox*, bool>> m_columnCheckboxes; // 存储列复选框及其状态
    QStringList m_filteredHeaders; // 存储筛选后的表头
    QList<QStringList> m_originalData; // 存储原始数据用于筛选
    bool m_isFiltered = false; // 标记是否处于筛选状态
};

#endif // MAINWINDOW_H