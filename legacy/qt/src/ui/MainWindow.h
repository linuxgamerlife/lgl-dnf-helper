#pragma once

#include <QColor>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>

#include "backend/Dnf5CliBackend.h"
#include "model/Dependency.h"
#include "model/Package.h"
#include "model/PackageQueryResult.h"
#include "services/PackageClassifier.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void runSearch();
    void resetUi();
    void goBack();
    void goForward();
    void onSearchCompleted(const QList<PackageQueryResult> &results);
    void onResultClicked(int row, int column);
    void onResultActivated(int row, int column);
    void onPackageLoaded(const QString &requestName, const Package &package);
    void onDependenciesLoaded(const QString &requestName, const QList<DependencyEdge> &dependencies);
    void onRequiredByLoaded(const QString &requestName, const QList<DependencyEdge> &dependencies);
    void onFilesLoaded(const QString &requestName, const QStringList &files);
    void onRelatedConfigLoaded(const QString &requestName, const QList<QStringList> &rows);
    void onHistoryLoaded(const QString &requestName, const QList<QStringList> &rows);
    void onRepositoryInfoLoaded(const QString &requestName, const QList<QStringList> &rows);
    void onImpactLoaded(const QString &requestName, const QList<QStringList> &rows);
    void onUserConfigLoaded(const QString &requestName, const QList<QStringList> &rows);
    void onToolsChecked(bool dnf5Available, bool rpmAvailable);
    void showTableContextMenu(const QPoint &pos);
    void loadCurrentTab();
    void showAbout();
    void clearLog();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setupUi();
    void setupMenuBar();
    void setupConnections();
    void buildSetupTab();
    void applySetupMode(const QString &message);
    void applyNormalMode();
    void setState(const QString &label, const QColor &color);
    void loadSelectedPackage(const QString &name, bool addToHistory = true);
    void navigateToPackage(const QString &name);
    void updateNavigationButtons();
    void clearPackageTables();
    void clearTable(QTableWidget *table);
    void appendLog(const QString &text, const QString &color = QString());
    void populatePackageHeader(const Package &package);
    void populateOverview(const Package &package);
    void populateDependencyTable(QTableWidget *table, const QList<DependencyEdge> &dependencies);
    void populateRows(QTableWidget *table, const QList<QStringList> &rows);
    void applyRowTooltip(QTableWidget *table, int row, const QString &tooltip);
    void adjustTableRows(QTableWidget *table);
    void markTabLoaded(const QString &key);
    bool shouldLoadTab(const QString &key);
    int resultIndexForTableRow(QTableWidget *table, int row) const;
    QString selectedTableText(QTableWidget *table) const;
    QString allTableText(QTableWidget *table) const;
    QString selectedDependencyLookup(QTableWidget *table) const;
    QString selectedUrl(QTableWidget *table) const;
    QString selectedLocalPath(QTableWidget *table) const;
    static QTableWidgetItem *item(const QString &text);

    Dnf5CliBackend *backend = nullptr;
    PackageClassifier classifier;

    QWidget *centralWidget = nullptr;
    QTabWidget *mainTabs = nullptr;
    QWidget *setupTab = nullptr;
    QWidget *overviewTab = nullptr;

    QLabel *statusDot = nullptr;
    QLabel *statusLabel = nullptr;
    QLabel *packageTitle = nullptr;
    QLabel *packageNevra = nullptr;
    QLabel *packageBadges = nullptr;
    QLineEdit *searchEdit = nullptr;
    QPushButton *backBtn = nullptr;
    QPushButton *forwardBtn = nullptr;
    QPushButton *searchBtn = nullptr;
    QPushButton *resetBtn = nullptr;

    QTableWidget *installedResultsTable = nullptr;
    QTableWidget *otherResultsTable = nullptr;
    QTableWidget *overviewTable = nullptr;
    QTextEdit *descriptionBox = nullptr;
    QTableWidget *dependenciesTable = nullptr;
    QTableWidget *requiredByTable = nullptr;
    QTableWidget *filesTable = nullptr;
    QTableWidget *relatedConfigTable = nullptr;
    QTableWidget *userConfigTable = nullptr;
    QTableWidget *historyTable = nullptr;
    QTableWidget *repositoryTable = nullptr;
    QTableWidget *impactTable = nullptr;
    QTextEdit *logView = nullptr;

    QList<PackageQueryResult> currentResults;
    QString currentPackageName;
    QStringList packageHistory;
    QSet<QString> loadedTabs;
    QSet<QString> loadingTabs;
    int packageHistoryIndex = -1;
    bool resetToOverviewOnSearchResult = false;
};
