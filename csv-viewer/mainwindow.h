#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QMessageBox>
#include <QAction>
#include <QTableView>

#include "CsvReader.h"
#include "TableModel.h"

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

private:
    // 加载CSV文件
    void loadCsvFile(const QString &filePath);
    
    // 显示CSV数据
    void displayCsvData(bool loadAll = false);
    
    Ui::MainWindow *ui;
    CsvReader *m_csvReader;
    TableModel *m_tableModel;
    
    // 性能优化相关成员
    const int DEFAULT_ROWS_LIMIT = 5000; // 默认初始加载行数限制
    int m_currentLoadedRows = 0; // 当前已加载的行数
};

#endif // MAINWINDOW_H