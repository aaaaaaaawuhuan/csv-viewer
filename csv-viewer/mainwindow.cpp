#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "TableModel.h"
#include "VirtualTableModel.h"
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
#include <QLineEdit>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_tableModel(new TableModel(this))
    , m_virtualTableModel(nullptr)
    , m_csvReader(new CsvReader(this))
    , m_verticalScrollBar(new QScrollBar(Qt::Vertical, this))
    , m_currentFilePath(QString())
    , m_isFiltered(false)
    , m_totalRows(0)
    , m_visibleRows(100)
{
    ui->setupUi(this);
    
    // 设置表格模型
    ui->tableView->setModel(m_tableModel);
    
    // 创建自定义垂直滚动条并添加到布局中
    m_verticalScrollBar->hide(); // 初始隐藏自定义滚动条
    connect(m_verticalScrollBar, &QScrollBar::valueChanged, this, &MainWindow::handleVerticalScroll);
    
    // 表格视图性能优化设置
    ui->tableView->setSortingEnabled(false); // 禁用排序，需要时再启用
    ui->tableView->setSelectionMode(QAbstractItemView::ExtendedSelection); // 设置选择模式
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers); // 禁用编辑
    ui->tableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel); // 像素滚动
    ui->tableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); // 像素滚动
    ui->tableView->setAttribute(Qt::WA_AlwaysShowToolTips);
    
    // 禁用QTableView的默认垂直滚动条
    ui->tableView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // 将自定义滚动条添加到界面布局中
    QGridLayout* gridLayout = qobject_cast<QGridLayout*>(ui->centralwidget->layout());
    if (gridLayout) {
        gridLayout->addWidget(m_verticalScrollBar, 0, 2); // 添加到网格布局的第0行第2列
    }
    
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
    
    // 连接dockWidget的可见性变化信号
    connect(ui->filterDockWidget, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        ui->actionShowFilterPanel->setChecked(visible);
    });
    
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
    delete m_virtualTableModel;
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
        // 保存当前的虚拟表格模型，避免重复创建
        VirtualTableModel* currentModel = m_virtualTableModel;
        m_virtualTableModel = nullptr; // 防止loadCsvFile中再次创建
        
        loadCsvFile(m_currentFilePath);
        
        m_virtualTableModel = currentModel; // 恢复原来的模型
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
        // 估算文件总行数
        m_totalRows = m_csvReader->estimateTotalRows();
        qDebug() << "Estimated total rows:" << m_totalRows;
        
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
    
    // 初始化虚拟表格模型（如果尚未初始化）
    if (!m_virtualTableModel) {
        m_virtualTableModel = new VirtualTableModel(m_csvReader, this);
    }
    
    // 清空现有数据
    m_currentLoadedRows = 0;
    
    // 重置筛选状态
    resetFilterPanel();
    
    // 保存表头
    QStringList headers = m_csvReader->getHeaders();
    
    // 设置表头
    m_virtualTableModel->setHeaders(headers);
    m_virtualTableModel->setTotalRowCount(m_totalRows);
    
    // 设置表格模型
    ui->tableView->setModel(m_virtualTableModel);
    
    // 设置自定义滚动条范围
    m_verticalScrollBar->setMinimum(0);
    m_verticalScrollBar->setMaximum(qMax(0, m_totalRows - m_visibleRows));
    m_verticalScrollBar->setPageStep(m_visibleRows);
    m_verticalScrollBar->setValue(0);
    
    // 显示自定义滚动条（需要添加到布局中）
    m_verticalScrollBar->show();
    
    // 设置筛选面板
    setupFilterPanel(headers);
    
    // 初始时隐藏所有列，直到用户点击筛选按钮
    for (int i = 0; i < headers.size(); ++i) {
        ui->tableView->setColumnHidden(i, true);
    }
    
    // 隐藏表格，直到用户点击筛选按钮
    ui->tableView->setVisible(false);
    statusBar()->showMessage(tr("请在左侧选择要显示的列，然后点击'筛选'按钮"));
    
    // 预加载初始数据
    m_virtualTableModel->loadDataRange(0, qMin(m_visibleRows + 50, m_totalRows));
    
    qint64 uiDisplayTime = uiDisplayTimer.elapsed();
    qDebug() << "UI display time (loading all data):" << uiDisplayTime << "ms";
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
            
            // 直接添加到表格模型中，列的可见性已经通过setColumnHidden设置
            m_tableModel->addRows(moreOriginalRows);
            
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
    
    // 增加筛选面板最小宽度，确保复选框文本完整显示
    ui->filterDockWidget->setMinimumWidth(180);
    ui->filterContentWidget->setMinimumWidth(170);
    
    // 添加搜索输入框
    m_searchLineEdit = new QLineEdit(this);
    m_searchLineEdit->setPlaceholderText(tr("搜索列名..."));
    m_searchLineEdit->setMinimumWidth(150);
    filterLayout->addWidget(m_searchLineEdit);
    
    // 连接搜索框的文本变化信号
    connect(m_searchLineEdit, &QLineEdit::textChanged, this, &MainWindow::filterCheckboxes);
    
    // 添加全选和清空按钮
    QHBoxLayout *controlButtonsLayout = new QHBoxLayout();
    QPushButton *selectAllButton = new QPushButton(tr("全选"), this);
    QPushButton *clearAllButton = new QPushButton(tr("清空"), this);
    
    // 设置按钮最小宽度，防止被过度压缩
    selectAllButton->setMinimumWidth(70);
    clearAllButton->setMinimumWidth(70);
    
    // 设置按钮样式
    selectAllButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    clearAllButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    connect(selectAllButton, &QPushButton::clicked, this, &MainWindow::selectAllColumns);
    connect(clearAllButton, &QPushButton::clicked, this, &MainWindow::clearAllColumns);
    
    // 让按钮在水平布局中均匀分布
    controlButtonsLayout->addWidget(selectAllButton);
    QSpacerItem* horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    controlButtonsLayout->addItem(horizontalSpacer);
    controlButtonsLayout->addWidget(clearAllButton);
    
    filterLayout->addLayout(controlButtonsLayout);
    
    // 添加列复选框
    for (int i = 0; i < headers.size(); ++i) {
        QCheckBox *checkBox = new QCheckBox(headers[i], this);
        checkBox->setChecked(true); // 默认选中所有列
        checkBox->setObjectName(QString("columnCheckBox_%1").arg(i));
        // 设置复选框的尺寸策略，确保文本不会被截断
        checkBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        checkBox->setMinimumWidth(150); // 确保有足够的宽度显示文本
        filterLayout->addWidget(checkBox);
        
        // 存储复选框及其状态
        m_columnCheckboxes.append(qMakePair(checkBox, true));
    }
    

    
    // 添加一个伸缩项，确保所有复选框都显示在顶部
    filterLayout->addStretch();
}

void MainWindow::filterCheckboxes(const QString &text)
{
    // 根据搜索文本过滤复选框显示
    for (const auto &pair : qAsConst(m_columnCheckboxes)) {
        QCheckBox *checkBox = pair.first;
        bool matches = text.isEmpty() || checkBox->text().contains(text, Qt::CaseInsensitive);
        checkBox->setVisible(matches);
    }
}

void MainWindow::handleVerticalScroll(int value)
{
    // 处理自定义垂直滚动条的值变化
    if (!m_virtualTableModel) {
        return;
    }
    
    // 记录上次滚动位置，避免重复加载相同数据
    static int lastStartRow = -1;
    static int lastRowCount = -1;
    
    // 如果滚动位置没有实质性变化，则不进行任何操作
    if (value == lastStartRow) {
        return;
    }
    
    lastStartRow = value;
    lastRowCount = qMin(m_visibleRows + 100, m_totalRows - value);
    
    // 计算需要加载的数据范围（增加预加载量）
    int startRow = value;
    int rowCount = qMin(m_visibleRows + 100, m_totalRows - startRow); // 增加预加载量到100行
    
    // 加载指定范围的数据到缓存
    m_virtualTableModel->loadDataRange(startRow, rowCount);
    
    // 使用更高效的刷新方式
    m_verticalScrollBar->setValue(value);
    ui->tableView->viewport()->update();
    
    // 异步加载更多数据（后台预加载）
    QMetaObject::invokeMethod(this, [this, value]() {
        // 加载更大范围的数据用于后台缓存
        int startRow = qMax(0, value - m_visibleRows);
        int rowCount = qMin(m_visibleRows * 3, m_totalRows - startRow);
        m_virtualTableModel->loadDataRange(startRow, rowCount);
    }, Qt::QueuedConnection);
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
    QVector<int> visibleColumns;
    qDebug() << "Colum" << ui->tableView->model()->columnCount();
    for (int i = 0; i < m_columnCheckboxes.size(); ++i) {
        bool isVisible = m_columnCheckboxes[i].first->isChecked();

        if (isVisible) {
            m_filteredHeaders.append(m_columnCheckboxes[i].first->text());
            visibleColumns.append(i);
        }
        ui->tableView->setColumnHidden(i, !isVisible); // 通过隐藏/显示列实现筛选
    }

    
    // 如果没有选中任何列，显示提示
    if (m_filteredHeaders.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请至少选择一列"));
        return;
    }
    
    // 更新筛选状态
    m_isFiltered = true;
    
    // 更新表头，确保表头与可见列一致
    //m_tableModel->setHeaders(m_filteredHeaders);
    
    // 显示表格
    ui->tableView->setVisible(true);
    
    // 优化列宽调整：大幅减少需要调整的列数，提高性能
    int columnCount = m_tableModel->columnCount();
    int columnsToResize = qMin(columnCount, 10); // 只调整前10列，显著提高性能
    for (int i = 0; i < columnsToResize; ++i) {
        if (!ui->tableView->isColumnHidden(i)) {
            ui->tableView->resizeColumnToContents(i);
        }
    }
    
    statusBar()->showMessage(tr("已筛选显示 %1 列数据").arg(m_filteredHeaders.size()));
}

/*
void MainWindow::updateFilteredColumns()
{
    // 该方法已废弃，现在通过隐藏列的方式实现筛选功能
    // 清空现有数据和表头
    m_tableModel->clear();
    
    // 如果没有原始数据或没有选中列，直接返回
    if (m_originalData.isEmpty() || m_filteredHeaders.isEmpty()) {
        return;
    }
    
    // 设置筛选后的表头
    m_tableModel->setHeaders(m_filteredHeaders);
    
    // 收集选中的列索引并确保与m_filteredHeaders一致
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
    
    // 优化刷新方式：使用更轻量级的刷新方法替代reset()以提高性能
    ui->tableView->setModel(nullptr);
    ui->tableView->setModel(m_tableModel);
    
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
}*/

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
