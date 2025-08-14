#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "TableModel.h"
#include "CsvReader.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_tableModel(new TableModel(this))
    , m_csvReader(new CsvReader(this))
{
    ui->setupUi(this);
    
    // 设置表格模型
    ui->tableView->setModel(m_tableModel);
    
    // 连接菜单项到打开文件槽函数
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open CSV File"), "", tr("CSV Files (*.csv)"));
    
    if (!fileName.isEmpty()) {
        loadCsvFile(fileName);
    }
}

void MainWindow::loadCsvFile(const QString &filePath)
{
    qDebug() << "Loading file:" << filePath;
    
    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        QString error = QString("File does not exist: %1").arg(filePath);
        qDebug() << error;
        QMessageBox::critical(this, tr("Error"), error);
        return;
    }
    
    // 检查文件是否可读
    if (!fileInfo.isReadable()) {
        QString error = QString("File is not readable: %1").arg(filePath);
        qDebug() << error;
        QMessageBox::critical(this, tr("Error"), error);
        return;
    }
    
    // 获取绝对路径
    QString absolutePath = fileInfo.absoluteFilePath();
    qDebug() << "Absolute file path:" << absolutePath;
    
    // 尝试加载文件
    if (m_csvReader->loadFile(absolutePath)) {
        displayCsvData();
        setWindowTitle(QString("CSV Viewer - %1").arg(fileInfo.fileName()));
    } else {
        QString error = QString("Failed to load file: %1\nError: %2")
            .arg(filePath)
            .arg(m_csvReader->getLastError());
        qDebug() << error;
        QMessageBox::critical(this, tr("Error"), error);
    }
}

void MainWindow::displayCsvData()
{
    // 清空现有数据
    m_tableModel->clear();
    
    // 设置表头
    m_tableModel->setHeaders(m_csvReader->getHeaders());
    
    // 添加数据行
    for (int i = 0; i < m_csvReader->getRowCount(); ++i) {
        m_tableModel->addRow(m_csvReader->getRow(i));
    }
    
    // 调整列宽
    ui->tableView->resizeColumnsToContents();
}
