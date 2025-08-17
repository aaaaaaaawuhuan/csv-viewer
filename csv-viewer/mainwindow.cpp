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
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_tableModel(new TableModel(this))
    , m_csvReader(new CsvReader(this))
    , m_currentFilePath(QString())
    , m_isFiltered(false)
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
    
    // 连接筛选按钮到槽函数
    connect(ui->filterButton, &QPushButton::clicked, this, &MainWindow::applyFilter);
    
    // 创建编码选择菜单
    createEncodingMenu();
    
    // 连接视图菜单中显示筛选面板的动作
    connect(ui->actionShowFilterPanel, &QAction::triggered, this, &MainWindow::toggleFilterPanel);
    
    // 确保筛选面板初始可见
    ui->actionShowFilterPanel->setChecked(true);
    ui->filterDockWidget->show();
    
    // 初始隐藏表格数据，直到用户点击筛选按钮
    ui->tableView->setVisible(false);
    statusBar()->showMessage(tr("请打开CSV文件，然后使用左侧面板筛选列"));
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
    
    // 重置筛选状态
    resetFilterPanel();
    
    // 保存表头和原始数据
    QStringList headers = m_csvReader->getHeaders();
    
    // 获取要加载的行数
    int totalRows = m_csvReader->getRowCount();
    int rowsToLoad = loadAll ? totalRows : qMin(DEFAULT_ROWS_LIMIT, totalRows);
    
    // 保存原始数据用于筛选
    if (rowsToLoad > 0) {
        m_originalData = m_csvReader->getRowsRange(0, rowsToLoad);
        m_currentLoadedRows = rowsToLoad;
    }
    
    // 设置筛选面板，但不显示表格数据
    setupFilterPanel(headers);
    
    // 隐藏表格，直到用户点击筛选按钮
    ui->tableView->setVisible(false);
    statusBar()->showMessage(tr("请在左侧选择要显示的列，然后点击'筛选'按钮"));
    
    qint64 uiDisplayTime = uiDisplayTimer.elapsed();
    qDebug() << "UI display time (loading headers only):" << uiDisplayTime << "ms";
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
            // 获取新加载的原始数据行
            QList<QStringList> moreOriginalRows = m_csvReader->getRowsRange(m_currentLoadedRows, newlyLoadedRows);
            
            // 将新加载的行添加到原始数据中
            m_originalData.append(moreOriginalRows);
            m_currentLoadedRows = totalRows;
            
            if (m_isFiltered) {
                // 如果当前处于筛选状态，需要重新应用筛选
                updateFilteredColumns();
            } else {
                // 否则直接添加到表格模型
                m_tableModel->addRows(moreOriginalRows);
            }
            
            qint64 loadMoreTime = loadMoreTimer.elapsed();
            qDebug() << "Loaded additional" << newlyLoadedRows << "rows in" << loadMoreTime << "ms";
        }
    } else {
        qDebug() << "Failed to load more rows: " << m_csvReader->getLastError();
    }
}

void MainWindow::setupFilterPanel(const QStringList &headers)
{
    // 清空现有布局中的所有组件
    resetFilterPanel();
    
    // 创建筛选布局
    QVBoxLayout *filterLayout = qobject_cast<QVBoxLayout*>(ui->filterContentWidget->layout());
    if (!filterLayout) {
        filterLayout = new QVBoxLayout(ui->filterContentWidget);
        ui->filterContentWidget->setLayout(filterLayout);
    }
    
    // 添加全选和清除按钮
    QHBoxLayout *controlButtonsLayout = new QHBoxLayout();
    QPushButton *selectAllButton = new QPushButton(tr("全选"), this);
    QPushButton *clearAllButton = new QPushButton(tr("清除"), this);
    
    connect(selectAllButton, &QPushButton::clicked, this, &MainWindow::selectAllColumns);
    connect(clearAllButton, &QPushButton::clicked, this, &MainWindow::clearAllColumns);
    
    controlButtonsLayout->addWidget(selectAllButton);
    controlButtonsLayout->addWidget(clearAllButton);
    
    filterLayout->addLayout(controlButtonsLayout);
    
    // 添加列复选框
    for (int i = 0; i < headers.size(); ++i) {
        QCheckBox *checkBox = new QCheckBox(headers[i], this);
        checkBox->setChecked(true); // 默认选中所有列
        checkBox->setObjectName(QString("columnCheckBox_%1").arg(i));
        filterLayout->addWidget(checkBox);
        
        // 存储复选框及其状态
        m_columnCheckboxes.append(qMakePair(checkBox, true));
    }
    
    // 添加一个伸缩项，确保所有复选框都显示在顶部
    filterLayout->addStretch();
}

void MainWindow::resetFilterPanel()
{
    // 清除所有复选框
    for (auto &pair : m_columnCheckboxes) {
        delete pair.first;
    }
    m_columnCheckboxes.clear();
    
    // 清除筛选状态
    m_isFiltered = false;
    m_filteredHeaders.clear();
    
    // 清空布局
    QVBoxLayout *filterLayout = qobject_cast<QVBoxLayout*>(ui->filterContentWidget->layout());
    if (filterLayout) {
        QLayoutItem *item;
        while ((item = filterLayout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }
    }
}

void MainWindow::applyFilter()
{
    // 收集选中的列
    m_filteredHeaders.clear();
    QVector<int> selectedColumns;
    
    for (int i = 0; i < m_columnCheckboxes.size(); ++i) {
        if (m_columnCheckboxes[i].first->isChecked()) {
            m_filteredHeaders.append(m_columnCheckboxes[i].first->text());
            selectedColumns.append(i);
        }
    }
    
    // 如果没有选中任何列，显示提示
    if (m_filteredHeaders.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请至少选择一列"));
        return;
    }
    
    // 更新筛选状态
    m_isFiltered = true;
    
    // 应用筛选并显示结果
    updateFilteredColumns();
    
    // 显示表格
    ui->tableView->setVisible(true);
    statusBar()->showMessage(tr("已筛选显示 %1 列数据").arg(m_filteredHeaders.size()));
}

void MainWindow::updateFilteredColumns()
{
    // 清空现有数据
    m_tableModel->clear();
    
    // 设置筛选后的表头
    m_tableModel->setHeaders(m_filteredHeaders);
    
    // 如果没有原始数据，直接返回
    if (m_originalData.isEmpty()) {
        return;
    }
    
    // 收集选中的列索引
    QVector<int> selectedColumns;
    for (int i = 0; i < m_columnCheckboxes.size(); ++i) {
        if (m_columnCheckboxes[i].first->isChecked()) {
            selectedColumns.append(i);
        }
    }
    
    // 筛选数据行
    QList<QStringList> filteredRows;
    for (const QStringList &row : m_originalData) {
        QStringList filteredRow;
        for (int colIndex : selectedColumns) {
            if (colIndex < row.size()) {
                filteredRow.append(row[colIndex]);
            } else {
                filteredRow.append(QString()); // 对于超出范围的列，添加空字符串
            }
        }
        filteredRows.append(filteredRow);
    }
    
    // 添加筛选后的数据行
    if (!filteredRows.isEmpty()) {
        m_tableModel->addRows(filteredRows);
    }
    
    // 优化列宽调整：大幅减少需要调整的列数，提高性能
    int columnCount = m_filteredHeaders.size();
    int columnsToResize = qMin(columnCount, 10); // 只调整前10列，显著提高性能
    for (int i = 0; i < columnsToResize; ++i) {
        ui->tableView->resizeColumnToContents(i);
    }
    
    // 对于剩余的列，设置固定宽度，避免大量列宽计算
    for (int i = columnsToResize; i < columnCount; ++i) {
        ui->tableView->setColumnWidth(i, 80); // 设置固定宽度为80像素
    }
}

void MainWindow::selectAllColumns()
{
    for (auto &pair : m_columnCheckboxes) {
        pair.first->setChecked(true);
        pair.second = true;
    }
}

void MainWindow::clearAllColumns()
{
    for (auto &pair : m_columnCheckboxes) {
        pair.first->setChecked(false);
        pair.second = false;
    }
}

void MainWindow::toggleFilterPanel(bool visible)
{
    // 根据传入的参数显示或隐藏筛选面板
    if (visible) {
        ui->filterDockWidget->show();
    } else {
        ui->filterDockWidget->hide();
    }
    
    // 确保动作的选中状态与面板的实际显示状态一致
    ui->actionShowFilterPanel->setChecked(visible);
}
