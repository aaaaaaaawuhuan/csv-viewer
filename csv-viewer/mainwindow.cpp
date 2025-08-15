#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "TableModel.h"
#include "CsvReader.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QElapsedTimer>
#include <QScrollBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_tableModel(new TableModel(this))
    , m_csvReader(new CsvReader(this))
{
    ui->setupUi(this);
    
    // 设置表格模型
    ui->tableView->setModel(m_tableModel);
    
    // 连接表格视图的垂直滚动条信号，实现滚动到底部时加载更多数据
    connect(ui->tableView->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        QScrollBar *scrollBar = ui->tableView->verticalScrollBar();
        if (scrollBar && scrollBar->value() == scrollBar->maximum()) {
            // 当滚动到最底部时，加载更多数据
            loadMoreRows();
        }
    });
    
    // 表格视图性能优化设置
    ui->tableView->setSortingEnabled(false); // 禁用排序，需要时再启用
    ui->tableView->setSelectionMode(QAbstractItemView::ExtendedSelection); // 设置选择模式
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers); // 禁用编辑
    ui->tableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel); // 像素滚动
    ui->tableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); // 像素滚动
    ui->tableView->setAttribute(Qt::WA_AlwaysShowToolTips);
    ui->tableView->setAttribute(Qt::WA_OpaquePaintEvent);
    ui->tableView->viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    
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
    
    // 重置当前已加载行数
    m_currentLoadedRows = 0;
    
    // 尝试加载文件
    if (m_csvReader->loadFile(absolutePath)) {
        displayCsvData(false); // 默认不加载全部数据
        setWindowTitle(QString("CSV Viewer - %1").arg(fileInfo.fileName()));
    } else {
        QString error = QString("Failed to load file: %1\nError: %2")
            .arg(filePath)
            .arg(m_csvReader->getLastError());
        qDebug() << error;
        QMessageBox::critical(this, tr("Error"), error);
    }
}

void MainWindow::displayCsvData(bool loadAll)
{
    // 计时：整个UI显示过程
    QElapsedTimer uiDisplayTimer;
    uiDisplayTimer.start();
    
    // 清空现有数据
    m_tableModel->clear();
    m_currentLoadedRows = 0;
    
    // 设置表头
    m_tableModel->setHeaders(m_csvReader->getHeaders());
    
    // 获取要加载的行数
    int totalRows = m_csvReader->getRowCount();
    int rowsToLoad = loadAll ? totalRows : qMin(DEFAULT_ROWS_LIMIT, totalRows);
    
    // 添加数据行
    if (rowsToLoad > 0) {
        QList<QStringList> rows = m_csvReader->getRowsRange(0, rowsToLoad);
        m_tableModel->addRows(rows);
        m_currentLoadedRows = rowsToLoad;
    }
    
    // 优化列宽调整：大幅减少需要调整的列数，提高性能
    int columnCount = m_csvReader->getHeaders().size();
    int columnsToResize = qMin(columnCount, 10); // 只调整前10列，显著提高性能
    for (int i = 0; i < columnsToResize; ++i) {
        ui->tableView->resizeColumnToContents(i);
    }
    
    // 对于剩余的列，设置固定宽度，避免大量列宽计算
    for (int i = columnsToResize; i < columnCount; ++i) {
        ui->tableView->setColumnWidth(i, 80); // 设置固定宽度为80像素
    }
    
    // 确保表格内容清晰可见（重置任何可能影响显示的设置）
    ui->tableView->setAlternatingRowColors(false);
    ui->tableView->setStyleSheet("/* 清空之前的样式表，使用系统默认样式 */");
    
    qint64 uiDisplayTime = uiDisplayTimer.elapsed();
    qDebug() << "UI display time:" << uiDisplayTime << "ms" << "(" << m_currentLoadedRows << " rows)";
}

void MainWindow::loadMoreRows()
{
    // 检查是否还有未加载的数据
    int totalRows = m_csvReader->getRowCount();
    if (m_currentLoadedRows >= totalRows) {
        return; // 已经加载了所有数据
    }
    
    // 计时：加载更多数据的过程
    QElapsedTimer loadMoreTimer;
    loadMoreTimer.start();
    
    // 计算要加载的行数，每次加载DEFAULT_ROWS_LIMIT行或剩余的所有行
    int remainingRows = totalRows - m_currentLoadedRows;
    int rowsToLoad = qMin(DEFAULT_ROWS_LIMIT / 2, remainingRows); // 每次加载默认限制的一半
    
    // 获取更多数据行
    QList<QStringList> moreRows = m_csvReader->getRowsRange(m_currentLoadedRows, rowsToLoad);
    
    // 添加到表格模型
    m_tableModel->addRows(moreRows);
    m_currentLoadedRows += rowsToLoad;
    
    qint64 loadMoreTime = loadMoreTimer.elapsed();
    qDebug() << "Loaded additional" << rowsToLoad << "rows in" << loadMoreTime << "ms";
}
