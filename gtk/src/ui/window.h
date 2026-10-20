#pragma once

#include "core/backend.h"
#include "core/commandrunner.h"
#include "core/settings.h"
#include "ui/datatable.h"

#include <giomm/simpleaction.h>
#include <gtkmm/aboutdialog.h>
#include <gtkmm/applicationwindow.h>
#include <gtkmm/box.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/notebook.h>
#include <gtkmm/revealer.h>
#include <gtkmm/searchentry.h>
#include <gtkmm/textview.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace dnfhelper {

class HelpWindow;
class ThemeWatcher;

// The main window. Port of src/ui/MainWindow.cpp of the Qt app: search, installed and all
// package results, the selected package's details in lazily loaded tabs, and a command log.
class Window final : public Gtk::ApplicationWindow {
public:
    explicit Window(const Glib::RefPtr<Gtk::Application> &application);
    ~Window() override;

private:
    enum class State { Starting, Querying, Ready, Error, SetupNeeded };
    enum class Tone { Plain, Positive, Neutral, Negative };
    // The detail tabs that load on demand.
    enum class Tab { Dependencies, RequiredBy, Files, SystemConfig, UserConfig, History,
                     Repository, Impact };

    void buildHeader();
    void applyTitleBar(bool system);
    void buildBody();
    Gtk::Widget &buildResults();
    Gtk::Widget &buildDetails();
    void buildSetupPage();
    void buildActions(const Glib::RefPtr<Gtk::Application> &application);
    void connectBackend();
    void watchAltTap(Gtk::Widget &widget);
    void toggleMainMenu();
    void focusPrimaryInput();

    void notify(const std::string &text, Tone tone, unsigned int timeoutMs);
    void setState(State state);
    void appendLog(const std::string &text, Tone tone = Tone::Plain);
    void updateLogColors();
    void showHelp();
    void showAbout();

    void runSearch();
    void resetUi();
    void onToolsChecked(bool dnf5Available, bool rpmAvailable);
    void applySetupMode(const std::string &message);
    void applyNormalMode();
    void onSearchCompleted(const Request &request, const std::vector<PackageQueryResult> &results);
    std::string resultTarget(const Glib::RefPtr<TableRow> &row) const;
    void openPackage(const std::string &name);
    void navigateFromDependency(const Row &row, bool requiredBy);
    void loadCurrentTab();
    bool shouldLoadTab(Tab tab);
    void markTabLoaded(Tab tab);
    Request currentRequest() const;
    bool isCurrent(const Request &request) const;
    void clearPackageTables();
    void clearPackageHeader(const std::string &hint);
    void populatePackageHeader(const Package &package);
    void populateOverview(const Package &package);
    void populateDependencies(DataTable &table, const std::vector<DependencyEdge> &dependencies);
    void populateRows(DataTable &table, const Rows &rows);

    Settings m_settings;
    SubprocessRunner m_runner;
    Backend m_backend;

    // A member, not managed, so it survives moving between the title bar and m_content.
    Gtk::HeaderBar m_header;
    Gtk::Box m_content;
    Gtk::Revealer m_noticeRevealer;
    Gtk::Box m_noticeBox;
    Gtk::Label m_notice;
    Gtk::Label m_status;
    Gtk::MenuButton *m_menuButton{nullptr};
    bool m_altTapped{false};

    Gtk::SearchEntry m_search;
    Gtk::Label m_installedCount;
    Gtk::Label m_allCount;
    DataTable m_installedResults;
    DataTable m_allResults;

    Gtk::Label m_packageTitle;
    Gtk::Label m_packageNevra;
    Gtk::Box m_badges;
    Gtk::Notebook m_tabs;
    Gtk::Box m_setupPage;
    Gtk::Widget *m_overviewPage{nullptr};
    DataTable m_overview;
    Gtk::TextView m_description;
    DataTable m_dependencies;
    DataTable m_requiredBy;
    DataTable m_files;
    DataTable m_systemConfig;
    DataTable m_userConfig;
    DataTable m_historyTable;
    DataTable m_repository;
    DataTable m_impact;
    Gtk::TextView m_log;
    std::map<const Gtk::Widget *, Tab> m_tabPages;

    std::unique_ptr<ThemeWatcher> m_theme;
    std::unique_ptr<HelpWindow> m_help;
    std::unique_ptr<Gtk::AboutDialog> m_about;
    Glib::RefPtr<Gio::SimpleAction> m_searchAction;
    sigc::connection m_noticeTimer;

    State m_state{State::Starting};
    std::vector<PackageQueryResult> m_results;
    std::string m_currentPackage;
    std::set<Tab> m_loadedTabs;
    std::set<Tab> m_loadingTabs;
    // Replies for an older package selection or search are dropped.
    std::uint64_t m_generation{0};
    std::uint64_t m_searchGeneration{0};
    std::uint64_t m_resolveGeneration{0};
    bool m_resetToOverviewOnSearchResult{false};
};

} // namespace dnfhelper
