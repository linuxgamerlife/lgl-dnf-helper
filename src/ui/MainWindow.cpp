#include "MainWindow.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>

#include "util/HumanSize.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("LGL DNF Helper");
    setMinimumSize(720, 560);
    resize(860, 640);

    backend = new Dnf5CliBackend(this);

    setupUi();
    setupMenuBar();
    setupConnections();
    buildSetupTab();
    qApp->installEventFilter(this);

    backend->checkTools();
}

void MainWindow::setupUi() {
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(12, 12, 12, 8);
    rootLayout->setSpacing(8);

    auto *headerFrame = new QFrame;
    auto *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(12, 8, 12, 8);

    auto *titleLabel = new QLabel("LGL DNF Helper");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    statusDot = new QLabel("●");
    QFont dotFont = statusDot->font();
    dotFont.setPointSize(16);
    statusDot->setFont(dotFont);
    statusDot->setToolTip("Backend status");

    statusLabel = new QLabel("Starting");

    refreshBtn = new QPushButton("↻ Refresh");
    refreshBtn->setFixedHeight(34);
    refreshBtn->setMinimumWidth(110);

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(statusDot);
    headerLayout->addWidget(statusLabel);
    headerLayout->addSpacing(8);
    headerLayout->addWidget(refreshBtn);
    rootLayout->addWidget(headerFrame);

    auto *searchRow = new QHBoxLayout;
    backBtn = new QPushButton("← Back");
    backBtn->setFixedHeight(34);
    backBtn->setToolTip("Go back to the previous package you opened");
    backBtn->setEnabled(false);
    forwardBtn = new QPushButton("Forward →");
    forwardBtn->setFixedHeight(34);
    forwardBtn->setToolTip("Go forward to the next package in your navigation history");
    forwardBtn->setEnabled(false);
    searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("Package, command, capability, or file path");
    searchBtn = new QPushButton("Search");
    searchBtn->setFixedHeight(34);
    searchBtn->setMinimumWidth(100);
    searchRow->addWidget(backBtn);
    searchRow->addWidget(forwardBtn);
    searchRow->addWidget(searchEdit);
    searchRow->addWidget(searchBtn);
    rootLayout->addLayout(searchRow);

    auto *contentSplitter = new QSplitter(Qt::Horizontal);
    contentSplitter->setChildrenCollapsible(false);
    contentSplitter->setHandleWidth(8);

    resultsTable = new QTableWidget;
    resultsTable->setColumnCount(3);
    resultsTable->setHorizontalHeaderLabels({"Package", "Match", "Repository"});
    resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    resultsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    resultsTable->verticalHeader()->setVisible(false);
    resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTable->setAlternatingRowColors(true);
    resultsTable->setShowGrid(false);
    resultsTable->setWordWrap(false);
    resultsTable->verticalHeader()->setDefaultSectionSize(28);
    resultsTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    resultsTable->setMinimumWidth(280);

    auto *detailWidget = new QWidget;
    auto *detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(8);

    auto *packageGroup = new QGroupBox("Selected Package");
    auto *packageLayout = new QVBoxLayout(packageGroup);
    packageTitle = new QLabel("No package selected");
    QFont packageTitleFont = packageTitle->font();
    packageTitleFont.setPointSize(12);
    packageTitleFont.setBold(true);
    packageTitle->setFont(packageTitleFont);
    packageNevra = new QLabel("Search for an installed package to begin.");
    packageBadges = new QLabel;
    packageBadges->setWordWrap(true);
    packageLayout->addWidget(packageTitle);
    packageLayout->addWidget(packageNevra);
    packageLayout->addWidget(packageBadges);
    detailLayout->addWidget(packageGroup);

    mainTabs = new QTabWidget;

    overviewTable = new QTableWidget;
    overviewTable->setColumnCount(2);
    overviewTable->setHorizontalHeaderLabels({"Field", "Value"});
    overviewTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    overviewTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    dependenciesTable = new QTableWidget;
    dependenciesTable->setColumnCount(5);
    dependenciesTable->setHorizontalHeaderLabels({"Type", "Capability", "Installed Provider", "Available Provider", "Repository"});
    dependenciesTable->setToolTip("Right-click rows to copy or inspect dependencies.");

    requiredByTable = new QTableWidget;
    requiredByTable->setColumnCount(5);
    requiredByTable->setHorizontalHeaderLabels({"Type", "Package/Capability", "Installed Reason", "Repository", "Risk"});
    requiredByTable->setToolTip("Right-click rows to copy or inspect packages that depend on this one.");

    filesTable = new QTableWidget;
    filesTable->setColumnCount(2);
    filesTable->setHorizontalHeaderLabels({"Type", "Path"});

    relatedConfigTable = new QTableWidget;
    relatedConfigTable->setColumnCount(4);
    relatedConfigTable->setHorizontalHeaderLabels({"Owner", "Path", "Kind", "Why shown"});

    userConfigTable = new QTableWidget;
    userConfigTable->setColumnCount(4);
    userConfigTable->setHorizontalHeaderLabels({"Type", "Path", "Kind", "Why shown"});

    historyTable = new QTableWidget;
    historyTable->setColumnCount(5);
    historyTable->setHorizontalHeaderLabels({"Transaction", "Date", "Action", "Version", "Command"});

    repositoryTable = new QTableWidget;
    repositoryTable->setColumnCount(4);
    repositoryTable->setHorizontalHeaderLabels({"Repo", "Status", "Version", "Vendor"});

    impactTable = new QTableWidget;
    impactTable->setColumnCount(2);
    impactTable->setHorizontalHeaderLabels({"Check", "Result"});

    logView = new QTextEdit;
    logView->setReadOnly(true);
    QFont monoFont;
    monoFont.setFamily("Monospace");
    monoFont.setPointSize(9);
    logView->setFont(monoFont);

    const QList<QTableWidget *> tables = {
        overviewTable, dependenciesTable, requiredByTable, filesTable, relatedConfigTable,
        userConfigTable, historyTable, repositoryTable, impactTable
    };
    for (QTableWidget *table : tables) {
        table->verticalHeader()->setVisible(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->setShowGrid(false);
        table->setWordWrap(false);
        table->verticalHeader()->setMinimumSectionSize(26);
        table->verticalHeader()->setDefaultSectionSize(28);
        table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        table->horizontalHeader()->setStretchLastSection(true);
        table->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(table, &QTableWidget::customContextMenuRequested, this, &MainWindow::showTableContextMenu);
    }
    overviewTable->setWordWrap(true);
    overviewTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    impactTable->setWordWrap(true);
    impactTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    mainTabs->addTab(overviewTable, "Overview");
    mainTabs->addTab(dependenciesTable, "Dependencies");
    mainTabs->addTab(requiredByTable, "Required By");
    mainTabs->addTab(filesTable, "Files");
    mainTabs->addTab(relatedConfigTable, "Related Config");
    mainTabs->addTab(userConfigTable, "Config");
    mainTabs->addTab(historyTable, "History");
    mainTabs->addTab(repositoryTable, "Repository");
    mainTabs->addTab(impactTable, "Impact");
    mainTabs->addTab(logView, "Log");

    detailLayout->addWidget(mainTabs);

    contentSplitter->addWidget(resultsTable);
    contentSplitter->addWidget(detailWidget);
    contentSplitter->setStretchFactor(0, 0);
    contentSplitter->setStretchFactor(1, 1);
    contentSplitter->setSizes({300, 760});
    rootLayout->addWidget(contentSplitter, 1);

    statusBar()->showMessage("Ready");
    setState("Starting", Qt::gray);
}

void MainWindow::setupMenuBar() {
    auto *aboutMenu = menuBar()->addMenu("About");
    auto *aboutAction = aboutMenu->addAction("About LGL DNF Helper");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    auto *logMenu = menuBar()->addMenu("Log");
    auto *clearAction = logMenu->addAction("Clear Log");
    connect(clearAction, &QAction::triggered, this, &MainWindow::clearLog);
}

void MainWindow::setupConnections() {
    connect(searchBtn, &QPushButton::clicked, this, &MainWindow::runSearch);
    connect(searchEdit, &QLineEdit::returnPressed, this, &MainWindow::runSearch);
    connect(backBtn, &QPushButton::clicked, this, &MainWindow::goBack);
    connect(forwardBtn, &QPushButton::clicked, this, &MainWindow::goForward);
    connect(refreshBtn, &QPushButton::clicked, backend, &Dnf5CliBackend::checkTools);
    connect(resultsTable, &QTableWidget::cellClicked, this, &MainWindow::onResultClicked);
    connect(resultsTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onResultActivated);
    connect(dependenciesTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        const QString lookup = selectedDependencyLookup(dependenciesTable);
        if (!lookup.isEmpty()) {
            searchEdit->setText(lookup);
            navigateToPackage(lookup);
        }
    });
    connect(requiredByTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        const QString lookup = selectedDependencyLookup(requiredByTable);
        if (!lookup.isEmpty()) {
            searchEdit->setText(lookup);
            navigateToPackage(lookup);
        }
    });
    connect(overviewTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        const QString url = selectedUrl(overviewTable);
        if (!url.isEmpty())
            QDesktopServices::openUrl(QUrl(url));
    });
    connect(mainTabs, &QTabWidget::currentChanged, this, [this](int) {
        loadCurrentTab();
    });

    connect(backend, &PackageBackend::toolsChecked, this, &MainWindow::onToolsChecked);
    connect(backend, &PackageBackend::searchCompleted, this, &MainWindow::onSearchCompleted);
    connect(backend, &PackageBackend::packageLoaded, this, &MainWindow::onPackageLoaded);
    connect(backend, &PackageBackend::dependenciesLoaded, this, &MainWindow::onDependenciesLoaded);
    connect(backend, &PackageBackend::requiredByLoaded, this, &MainWindow::onRequiredByLoaded);
    connect(backend, &PackageBackend::filesLoaded, this, &MainWindow::onFilesLoaded);
    connect(backend, &PackageBackend::relatedConfigLoaded, this, &MainWindow::onRelatedConfigLoaded);
    connect(backend, &PackageBackend::historyLoaded, this, &MainWindow::onHistoryLoaded);
    connect(backend, &PackageBackend::repositoryInfoLoaded, this, &MainWindow::onRepositoryInfoLoaded);
    connect(backend, &PackageBackend::impactLoaded, this, &MainWindow::onImpactLoaded);
    connect(backend, &PackageBackend::userConfigLoaded, this, &MainWindow::onUserConfigLoaded);
    connect(backend, &PackageBackend::commandStarted, this, [this](const QString &command) {
        setState("Querying", QColor("#ff9900"));
        appendLog("Running: " + command, "#00aa00");
        statusBar()->showMessage("Querying package data...");
    });
    connect(backend, &PackageBackend::commandFinished, this, [this](const QString &summary, bool ok) {
        appendLog(summary, ok ? "#00aa00" : "#ff4444");
        setState(ok ? "Ready" : "Error", ok ? QColor("green") : QColor("red"));
    });
    connect(backend, &PackageBackend::errorOccurred, this, [this](const QString &message) {
        appendLog(message, "#ff4444");
        statusBar()->showMessage(message, 5000);
    });
}

void MainWindow::buildSetupTab() {
    setupTab = new QWidget;
    auto *layout = new QVBoxLayout(setupTab);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    auto *titleLabel = new QLabel("⚠ DNF5 not detected");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(13);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto *bodyLabel = new QLabel(
        "LGL DNF Helper needs <b>dnf5</b> for dependency and repository queries.<br><br>"
        "Install the required tools on Fedora with:<br>"
        "<code>sudo dnf install dnf5 rpm</code><br><br>"
        "The app runs read-only package inspection as your regular user. "
        "Do not launch the GUI with sudo."
    );
    bodyLabel->setTextFormat(Qt::RichText);
    bodyLabel->setWordWrap(true);

    layout->addStretch();
    layout->addWidget(titleLabel);
    layout->addWidget(bodyLabel);
    layout->addStretch();
}

void MainWindow::applySetupMode(const QString &message) {
    if (mainTabs->indexOf(setupTab) == -1)
        mainTabs->insertTab(0, setupTab, "⚠ Setup");
    mainTabs->setCurrentWidget(setupTab);
    searchBtn->setEnabled(false);
    setState("Setup needed", QColor("#ff9900"));
    statusBar()->showMessage(message);
}

void MainWindow::applyNormalMode() {
    const int setupIndex = mainTabs->indexOf(setupTab);
    if (setupIndex != -1)
        mainTabs->removeTab(setupIndex);
    searchBtn->setEnabled(true);
    setState("Ready", QColor("green"));
    statusBar()->showMessage("Ready");
}

void MainWindow::runSearch() {
    const QString query = searchEdit->text().trimmed();
    if (query.isEmpty()) {
        statusBar()->showMessage("Enter a package, capability, or file path", 3000);
        return;
    }
    currentResults.clear();
    resultsTable->setRowCount(0);
    resetToOverviewOnSearchResult = true;
    backend->search(query);
}

void MainWindow::goBack() {
    if (packageHistoryIndex <= 0)
        return;
    --packageHistoryIndex;
    searchEdit->setText(packageHistory[packageHistoryIndex]);
    loadSelectedPackage(packageHistory[packageHistoryIndex], false);
}

void MainWindow::goForward() {
    if (packageHistoryIndex < 0 || packageHistoryIndex >= packageHistory.size() - 1)
        return;
    ++packageHistoryIndex;
    searchEdit->setText(packageHistory[packageHistoryIndex]);
    loadSelectedPackage(packageHistory[packageHistoryIndex], false);
}

void MainWindow::onSearchCompleted(const QList<PackageQueryResult> &results) {
    currentResults = results;
    resultsTable->setRowCount(results.size());

    for (int row = 0; row < results.size(); ++row) {
        const auto &result = results[row];
        resultsTable->setItem(row, 0, item(result.package.id.name));
        resultsTable->setItem(row, 1, item(matchKindLabel(result.matchKind)));
        resultsTable->setItem(row, 2, item(result.package.repoId));
    }

    adjustTableRows(resultsTable);
    statusBar()->showMessage(QString("%1 result(s)").arg(results.size()), 3000);

    if (resetToOverviewOnSearchResult) {
        mainTabs->setCurrentWidget(overviewTable);
        resetToOverviewOnSearchResult = false;
    }

    if (results.size() == 1)
        navigateToPackage(results.first().package.id.name);
}

void MainWindow::onResultClicked(int row, int) {
    if (row < 0 || row >= currentResults.size())
        return;
    loadSelectedPackage(currentResults[row].package.id.name, false);
}

void MainWindow::onResultActivated(int row, int) {
    if (row < 0 || row >= currentResults.size())
        return;
    navigateToPackage(currentResults[row].package.id.name);
}

void MainWindow::loadSelectedPackage(const QString &name, bool addToHistory) {
    if (name.trimmed().isEmpty())
        return;

    if (addToHistory) {
        if (packageHistoryIndex < 0 || packageHistory.value(packageHistoryIndex) != name) {
            while (packageHistory.size() > packageHistoryIndex + 1)
                packageHistory.removeLast();
            packageHistory << name;
            packageHistoryIndex = packageHistory.size() - 1;
        }
    }

    currentPackageName = name;
    loadedTabs.clear();
    clearPackageTables();
    updateNavigationButtons();
    packageTitle->setText(name);
    packageNevra->setText("Loading package details...");
    backend->loadPackage(name);
    loadCurrentTab();
}

void MainWindow::navigateToPackage(const QString &name) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;

    if (packageHistoryIndex >= 0 && packageHistory.value(packageHistoryIndex) == trimmed) {
        searchEdit->setText(trimmed);
        loadSelectedPackage(trimmed, false);
        return;
    }

    searchEdit->setText(trimmed);
    loadSelectedPackage(trimmed, true);
}

void MainWindow::updateNavigationButtons() {
    backBtn->setEnabled(packageHistoryIndex > 0);
    forwardBtn->setEnabled(packageHistoryIndex >= 0 && packageHistoryIndex < packageHistory.size() - 1);
}

void MainWindow::loadCurrentTab() {
    if (currentPackageName.isEmpty())
        return;

    QWidget *tab = mainTabs->currentWidget();
    QString key;
    if (tab == dependenciesTable) {
        key = "dependencies";
        if (!loadedTabs.contains(key)) backend->loadDependencies(currentPackageName);
    } else if (tab == requiredByTable) {
        key = "required-by";
        if (!loadedTabs.contains(key)) backend->loadRequiredBy(currentPackageName);
    } else if (tab == filesTable) {
        key = "files";
        if (!loadedTabs.contains(key)) backend->loadFiles(currentPackageName);
    } else if (tab == relatedConfigTable) {
        key = "related-config";
        if (!loadedTabs.contains(key)) backend->loadRelatedConfig(currentPackageName);
    } else if (tab == userConfigTable) {
        key = "config";
        if (!loadedTabs.contains(key)) backend->loadUserConfig(currentPackageName);
    } else if (tab == historyTable) {
        key = "history";
        if (!loadedTabs.contains(key)) backend->loadHistory(currentPackageName);
    } else if (tab == repositoryTable) {
        key = "repository";
        if (!loadedTabs.contains(key)) backend->loadRepositoryInfo(currentPackageName);
    } else if (tab == impactTable) {
        key = "impact";
        if (!loadedTabs.contains(key)) backend->loadImpact(currentPackageName);
    } else {
        return;
    }

    loadedTabs.insert(key);
}

void MainWindow::clearPackageTables() {
    clearTable(dependenciesTable);
    clearTable(requiredByTable);
    clearTable(filesTable);
    clearTable(relatedConfigTable);
    clearTable(userConfigTable);
    clearTable(historyTable);
    clearTable(repositoryTable);
    clearTable(impactTable);
}

void MainWindow::clearTable(QTableWidget *table) {
    table->setRowCount(0);
}

void MainWindow::onPackageLoaded(const QString &requestName, const Package &package) {
    if (requestName != currentPackageName)
        return;
    populatePackageHeader(package);
    populateOverview(package);
}

void MainWindow::onDependenciesLoaded(const QString &requestName, const QList<DependencyEdge> &dependencies) {
    if (requestName != currentPackageName)
        return;
    populateDependencyTable(dependenciesTable, dependencies);
}

void MainWindow::onRequiredByLoaded(const QString &requestName, const QList<DependencyEdge> &dependencies) {
    if (requestName != currentPackageName)
        return;
    populateDependencyTable(requiredByTable, dependencies);
}

void MainWindow::onFilesLoaded(const QString &requestName, const QStringList &files) {
    if (requestName != currentPackageName)
        return;
    filesTable->setRowCount(files.size());
    for (int row = 0; row < files.size(); ++row) {
        const QString &path = files[row];
        QString type = "Other";
        if (path.startsWith("/usr/bin") || path.startsWith("/usr/sbin"))
            type = "Executable";
        else if (path.contains("/lib") && path.endsWith(".so"))
            type = "Library";
        else if (path.startsWith("/etc"))
            type = "Config";
        else if (path.contains("/doc/"))
            type = "Documentation";
        else if (path.contains("/licenses/"))
            type = "License";
        else if (path.endsWith(".desktop"))
            type = "Desktop Entry";

        filesTable->setItem(row, 0, item(type));
        filesTable->setItem(row, 1, item(path));
    }
    adjustTableRows(filesTable);
}

void MainWindow::onRelatedConfigLoaded(const QString &requestName, const QList<QStringList> &rows) {
    if (requestName != currentPackageName)
        return;
    populateRows(relatedConfigTable, rows);
}

void MainWindow::onHistoryLoaded(const QString &requestName, const QList<QStringList> &rows) {
    if (requestName != currentPackageName)
        return;
    populateRows(historyTable, rows);
}

void MainWindow::onRepositoryInfoLoaded(const QString &requestName, const QList<QStringList> &rows) {
    if (requestName != currentPackageName)
        return;
    populateRows(repositoryTable, rows);
}

void MainWindow::onImpactLoaded(const QString &requestName, const QList<QStringList> &rows) {
    if (requestName != currentPackageName)
        return;
    populateRows(impactTable, rows);
}

void MainWindow::onUserConfigLoaded(const QString &requestName, const QList<QStringList> &rows) {
    if (requestName != currentPackageName)
        return;
    populateRows(userConfigTable, rows);
}

void MainWindow::onToolsChecked(bool dnf5Available, bool rpmAvailable) {
    appendLog(QString("Tool check: dnf5=%1 rpm=%2")
                  .arg(dnf5Available ? "yes" : "no", rpmAvailable ? "yes" : "no"));
    if (!dnf5Available) {
        applySetupMode("dnf5 not found on PATH");
        return;
    }
    if (!rpmAvailable)
        appendLog("rpm not found: file ownership views will be limited", "#ff9900");
    applyNormalMode();
}

void MainWindow::showTableContextMenu(const QPoint &pos) {
    auto *table = qobject_cast<QTableWidget *>(sender());
    if (!table)
        return;

    const QModelIndex index = table->indexAt(pos);
    if (!index.isValid())
        return;
    table->selectRow(index.row());

    QMenu menu(this);
    QAction *copyCell = menu.addAction("Copy Cell");
    QAction *copyRow = menu.addAction("Copy Row");
    QAction *copyAll = menu.addAction("Copy Table");
    QAction *openUrlAction = nullptr;
    const QString url = selectedUrl(table);
    if (!url.isEmpty())
        openUrlAction = menu.addAction("Open URL");

    QAction *inspect = nullptr;
    if (table == dependenciesTable || table == requiredByTable)
        inspect = menu.addAction("Inspect Selected Package/Capability");

    QAction *chosen = menu.exec(table->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == copyCell) {
        auto *cell = table->item(index.row(), index.column());
        QApplication::clipboard()->setText(cell ? cell->text() : QString());
        statusBar()->showMessage("Copied cell", 2000);
    } else if (chosen == copyRow) {
        QApplication::clipboard()->setText(selectedTableText(table));
        statusBar()->showMessage("Copied row", 2000);
    } else if (chosen == copyAll) {
        QApplication::clipboard()->setText(allTableText(table));
        statusBar()->showMessage("Copied table", 2000);
    } else if (chosen == openUrlAction) {
        QDesktopServices::openUrl(QUrl(url));
    } else if (chosen == inspect) {
        const QString lookup = selectedDependencyLookup(table);
        if (!lookup.isEmpty()) {
            searchEdit->setText(lookup);
            navigateToPackage(lookup);
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    Q_UNUSED(watched);
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::BackButton) {
            goBack();
            return true;
        }
        if (mouseEvent->button() == Qt::ForwardButton) {
            goForward();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::BackButton) {
        goBack();
        event->accept();
        return;
    }
    if (event->button() == Qt::ForwardButton) {
        goForward();
        event->accept();
        return;
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::showAbout() {
    QMessageBox about(this);
    about.setWindowTitle("About LGL DNF Helper");
    about.setTextFormat(Qt::RichText);
    about.setTextInteractionFlags(Qt::TextBrowserInteraction);
    about.setText(
        "<h3>LGL DNF Helper</h3>"
        "<p style='color:gray;'>Version 0.1.0</p>"
        "<p>A Qt6 GUI for understanding installed RPM packages, DNF5 dependencies, "
        "reverse dependencies, package files, and repository origin.</p>"
        "<hr/>"
        "<p><b>Created by LinuxGamerLife</b></p>"
        "<p><a href='https://linuxgamerlife.net'>linuxgamerlife.net</a> &nbsp;|&nbsp; "
        "<a href='https://github.com/linuxgamerlife'>github.com/linuxgamerlife</a></p>"
        "<hr/>"
        "<p style='color:gray; font-size:small;'>Read-only prototype. Released under the MIT licence.</p>"
    );
    about.exec();
}

void MainWindow::clearLog() {
    logView->clear();
}

void MainWindow::setState(const QString &label, const QColor &color) {
    statusLabel->setText(label);
    statusDot->setStyleSheet(QString("color:%1;").arg(color.name()));
}

void MainWindow::appendLog(const QString &text, const QString &color) {
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    const QString c = color.isEmpty() ? palette().text().color().name() : color;
    logView->append(QString("<span style='color:%1'>[%2] %3</span>")
                        .arg(c, ts, text.toHtmlEscaped()));
}

void MainWindow::populatePackageHeader(const Package &package) {
    packageTitle->setText(package.id.name);
    packageNevra->setText(package.id.nevra());
    packageBadges->setText(classifier.badgesForPackage(package).join("  "));
}

void MainWindow::populateOverview(const Package &package) {
    struct Row { QString key; QString value; };
    const QList<Row> rows = {
        {"Name", package.id.name},
        {"Version", package.id.evr()},
        {"Architecture", package.id.arch},
        {"Summary", package.summary},
        {"Repository", package.repoId},
        {"Install reason", package.installReason},
        {"Installed size", package.installSize > 0 ? humanSize(package.installSize) : QString()},
        {"Install time", package.installTime.isValid() ? package.installTime.toString(Qt::ISODate) : QString()},
        {"License", package.license},
        {"URL", package.url},
        {"Source RPM", package.sourceRpm},
        {"Vendor", package.vendor},
        {"Packager", package.packager},
    };

    overviewTable->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        overviewTable->setItem(row, 0, item(rows[row].key));
        overviewTable->setItem(row, 1, item(rows[row].value.isEmpty() ? "—" : rows[row].value));
    }
    adjustTableRows(overviewTable);
}

void MainWindow::populateDependencyTable(QTableWidget *table, const QList<DependencyEdge> &dependencies) {
    table->setRowCount(dependencies.size());
    for (int row = 0; row < dependencies.size(); ++row) {
        const auto &dependency = dependencies[row];
        table->setItem(row, 0, item(dependencyTypeLabel(dependency.type)));
        table->setItem(row, 1, item(dependency.capability));
        table->setItem(row, 2, item(dependency.installedProvider.name));
        table->setItem(row, 3, item(dependency.availableProviders.isEmpty() ? QString() : dependency.availableProviders.first().name));
        table->setItem(row, 4, item(QString()));
    }
    adjustTableRows(table);
}

void MainWindow::populateRows(QTableWidget *table, const QList<QStringList> &rows) {
    table->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        const QStringList &fields = rows[row];
        for (int column = 0; column < table->columnCount(); ++column) {
            table->setItem(row, column, item(column < fields.size() ? fields[column] : QString()));
        }
        if (table == impactTable && !fields.isEmpty() && fields.first() == "Leaf package?") {
            applyRowTooltip(table, row, "A leaf is an installed package that no other installed package depends on. It may be easier to remove, but it can still be an app you use.");
        }
    }
    adjustTableRows(table);
}

void MainWindow::applyRowTooltip(QTableWidget *table, int row, const QString &tooltip) {
    for (int column = 0; column < table->columnCount(); ++column) {
        if (auto *tableItem = table->item(row, column))
            tableItem->setToolTip(tooltip);
    }
}

void MainWindow::adjustTableRows(QTableWidget *table) {
    if (table == overviewTable || table == impactTable) {
        table->resizeRowsToContents();
        return;
    }

    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    for (int row = 0; row < table->rowCount(); ++row)
        table->setRowHeight(row, 28);
}

QString MainWindow::selectedTableText(QTableWidget *table) const {
    const int row = table->currentRow();
    if (row < 0)
        return QString();

    QStringList fields;
    for (int column = 0; column < table->columnCount(); ++column) {
        if (auto *tableItem = table->item(row, column))
            fields << tableItem->text();
    }
    return fields.join("\t");
}

QString MainWindow::allTableText(QTableWidget *table) const {
    QStringList lines;
    QStringList headers;
    for (int column = 0; column < table->columnCount(); ++column) {
        auto *header = table->horizontalHeaderItem(column);
        headers << (header ? header->text() : QString());
    }
    lines << headers.join("\t");

    for (int row = 0; row < table->rowCount(); ++row) {
        QStringList fields;
        for (int column = 0; column < table->columnCount(); ++column) {
            if (auto *tableItem = table->item(row, column))
                fields << tableItem->text();
            else
                fields << QString();
        }
        lines << fields.join("\t");
    }

    return lines.join("\n");
}

QString MainWindow::selectedDependencyLookup(QTableWidget *table) const {
    const int row = table->currentRow();
    if (row < 0)
        return QString();

    if (table == dependenciesTable) {
        if (auto *provider = table->item(row, 2); provider && provider->text() != "—")
            return provider->text();
        if (auto *available = table->item(row, 3); available && available->text() != "—")
            return available->text();
        if (auto *capability = table->item(row, 1); capability && capability->text() != "—")
            return capability->text();
    }

    if (table == requiredByTable) {
        if (auto *package = table->item(row, 1); package && package->text() != "—")
            return package->text();
    }

    return QString();
}

QString MainWindow::selectedUrl(QTableWidget *table) const {
    const int row = table->currentRow();
    if (row < 0)
        return QString();

    for (int column = 0; column < table->columnCount(); ++column) {
        auto *tableItem = table->item(row, column);
        if (!tableItem)
            continue;
        const QString text = tableItem->text().trimmed();
        if (text.startsWith("http://") || text.startsWith("https://"))
            return text;
    }

    return QString();
}

QTableWidgetItem *MainWindow::item(const QString &text) {
    auto *tableItem = new QTableWidgetItem(text.isEmpty() ? "—" : text);
    tableItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
    if (text.startsWith("http://") || text.startsWith("https://")) {
        tableItem->setForeground(QColor("#1d66c2"));
        tableItem->setToolTip("Double-click or right-click to open this URL");
    }
    return tableItem;
}
