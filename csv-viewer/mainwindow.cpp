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
    , m_currentFilePath(QString())
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
    
    // 设置表格字体为支持中文的字体
    QFont font = ui->tableView->font();
    font.setFamily(QStringLiteral("SimHei")); // 黑体
    ui->tableView->setFont(font);
    
    // 连接菜单项到打开文件槽函数
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
    
    // 创建编码选择菜单
    createEncodingMenu();
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

void MainWindow::createEncodingMenu()
{
    // 创建编码菜单
    QMenu *encodingMenu = new QMenu(tr("编码"), this);
    ui->menubar->addMenu(encodingMenu);
    
    // 创建编码动作组
    QActionGroup *encodingGroup = new QActionGroup(this);
    encodingGroup->setExclusive(true);
    
    // 添加UTF-8编码选项
    QAction *utf8Action = encodingMenu->addAction(tr("UTF-8"));
    utf8Action->setCheckable(true);
    utf8Action->setChecked(true); // 默认选中UTF-8
    encodingGroup->addAction(utf8Action);
    
    // 添加GBK编码选项
    QAction *gbkAction = encodingMenu->addAction(tr("GBK"));
    gbkAction->setCheckable(true);
    encodingGroup->addAction(gbkAction);
    
    // 添加自动检测编码选项
    QAction *autoDetectAction = encodingMenu->addAction(tr("自动检测"));
    autoDetectAction->setCheckable(true);
    encodingGroup->addAction(autoDetectAction);
    
    // 连接编码选择信号
    connect(utf8Action, &QAction::triggered, this, [this]() {
        m_csvReader->setEncoding(CsvReader::UTF8);
        qDebug() << "Encoding set to UTF-8";
        reloadCurrentFileIfNeeded();
    });
    
    connect(gbkAction, &QAction::triggered, this, [this]() {
        m_csvReader->setEncoding(CsvReader::GBK);
        qDebug() << "Encoding set to GBK";
        reloadCurrentFileIfNeeded();
    });
    
    connect(autoDetectAction, &QAction::triggered, this, [this]() {
        m_csvReader->setEncoding(CsvReader::AutoDetect);
        qDebug() << "Encoding set to AutoDetect";
        reloadCurrentFileIfNeeded();
    });
}

void MainWindow::reloadCurrentFileIfNeeded()
{
    // 如果当前已经打开了文件，则重新加载
    if (!m_currentFilePath.isEmpty()) {
        loadCsvFile(m_currentFilePath);
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
    
    // 保存当前文件路径，用于可能的重新加载
    m_currentFilePath = absolutePath;
    
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
    // 检查是否还有未加载的数据（使用CsvReader的延迟加载功能）
    if (!m_csvReader->hasMoreData()) {
        return; // 已经加载了所有数据或没有更多数据可加载
    }
    
    // 计时：加载更多数据的过程
    QElapsedTimer loadMoreTimer;
    loadMoreTimer.start();
    
    // 定义每次加载的行数
    const int ROWS_PER_LOAD = DEFAULT_ROWS_LIMIT / 4; // 每次加载默认限制的四分之一
    
    // 加载更多数据行
    if (m_csvReader->loadMoreRows(ROWS_PER_LOAD)) {
        // 获取已加载数据的总行数
        int totalRows = m_csvReader->getRowCount();
        
        // 计算新加载的行数
        int newlyLoadedRows = totalRows - m_currentLoadedRows;
        
        if (newlyLoadedRows > 0) {
            // 获取新加载的数据行
            QList<QStringList> moreRows = m_csvReader->getRowsRange(m_currentLoadedRows, newlyLoadedRows);
            
            // 添加到表格模型
            m_tableModel->addRows(moreRows);
            m_currentLoadedRows = totalRows;
            
            qint64 loadMoreTime = loadMoreTimer.elapsed();
            qDebug() << "Loaded additional" << newlyLoadedRows << "rows in" << loadMoreTime << "ms";
        }
    } else {
        qDebug() << "Failed to load more rows: " << m_csvReader->getLastError();
    }
}
