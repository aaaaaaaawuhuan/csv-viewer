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

private:
    // 加载CSV文件
    void loadCsvFile(const QString &filePath);
    
    // 显示CSV数据
    void displayCsvData();
    
    Ui::MainWindow *ui;
    CsvReader *m_csvReader;
    TableModel *m_tableModel;
};

#endif // MAINWINDOW_H