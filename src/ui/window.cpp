#include "ui/window.h"

#include "ui/helpwindow.h"
#include "ui/themewatcher.h"

#include <gdk/gdkkeysyms.h>
#include <gdkmm/texture.h>
#include <giomm/menu.h>
#include <glibmm/datetime.h>
#include <glibmm/main.h>
#include <glibmm/markup.h>
#include <gtkmm/button.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/image.h>
#include <gtkmm/overlay.h>
#include <gtkmm/paned.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/texttag.h>

#include <algorithm>

namespace dnfhelper {

namespace {

constexpr unsigned int routineNoticeMs = 5000;
constexpr unsigned int readingNoticeMs = 10000;

// The fixed status colours (light, dark) of the LGL GTK standard.
struct StatusColor {
    const char *light;
    const char *dark;
};
constexpr StatusColor positiveColor{"#067647", "#47cd89"};
constexpr StatusColor neutralColor{"#a15c07", "#fdb022"};
constexpr StatusColor negativeColor{"#b42318", "#f97066"};

const char *const leafTooltip =
    "A leaf is an installed package that no other installed package depends on. It may be "
    "easier to remove, but it can still be an app you use.";
const char *const buildRequiresTooltip =
    "Needed to build the package from source, not to run it.";

bool isAlt(const guint keyval)
{
    return keyval == GDK_KEY_Alt_L || keyval == GDK_KEY_Alt_R;
}

Glib::RefPtr<Gdk::Texture> appIcon()
{
    static const auto icon = Gdk::Texture::create_from_resource(
        "/com/linuxgamerlife/dnfhelper/lgl-dnf-helper.png");
    return icon;
}

const char *toneClass(const int tone)
{
    switch (tone) {
    case 1: return "positive";
    case 2: return "neutral";
    case 3: return "negative";
    default: return nullptr;
    }
}

Gtk::Label &sectionLabel(const std::string &text)
{
    auto *label = Gtk::make_managed<Gtk::Label>(text);
    label->add_css_class("section-label");
    label->set_xalign(0.0f);
    return *label;
}

// A flush card with a heading row (and an optional count on the right) over its content.
Gtk::Box &flushCard(const std::string &title, Gtk::Widget &content, Gtk::Label *count = nullptr)
{
    auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    card->add_css_class("dh-card");
    card->add_css_class("flush");
    card->set_overflow(Gtk::Overflow::HIDDEN);
    auto *head = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    head->add_css_class("card-head");
    auto &label = sectionLabel(title);
    label.set_hexpand(true);
    head->append(label);
    if (count) {
        count->add_css_class("mono");
        count->add_css_class("small");
        count->add_css_class("muted");
        head->append(*count);
    }
    card->append(*head);
    content.set_vexpand(true);
    card->append(content);
    return *card;
}

std::string countText(const guint count)
{
    return count == 0 ? std::string() : std::to_string(count);
}

} // namespace

Window::Window(const Glib::RefPtr<Gtk::Application> &application)
    : m_backend(m_runner)
    , m_content(Gtk::Orientation::VERTICAL)
    , m_noticeBox(Gtk::Orientation::HORIZONTAL)
    , m_installedResults({"Package"})
    , m_allResults({"Package", "Version", "Architecture"}, false, 0)
    , m_badges(Gtk::Orientation::HORIZONTAL, 6)
    , m_setupPage(Gtk::Orientation::VERTICAL, 12)
    , m_overview({"Field", "Value"}, true)
    , m_dependencies({"Type", "Capability", "Installed Provider", "Available Provider",
                      "Repository"})
    , m_requiredBy({"Type", "Package/Capability", "Installed Reason", "Repository", "Risk"})
    , m_files({"Type", "Path"})
    , m_systemConfig({"Owner", "Path", "Kind", "Why shown"})
    , m_userConfig({"Type", "Path", "Kind", "Why shown"})
    , m_historyTable({"Transaction", "Date", "Action", "Version", "Command"})
    , m_repository({"Repo", "Status", "Version", "Vendor"})
    , m_impact({"Check", "Result"}, true)
{
    set_application(application);
    set_title("LGL DNF Helper");
    set_icon_name("lgl-dnf-helper");
    set_default_size(1180, 760);
    set_size_request(720, 560);

    m_theme = std::make_unique<ThemeWatcher>(*this);
    buildHeader();
    buildBody();
    buildActions(application);
    applyTitleBar(m_settings.systemTitleBar());
    // The open menu is a separate surface that receives its own key events.
    watchAltTap(*this);
    watchAltTap(*m_menuButton->get_popover());
    connectBackend();

    m_theme->signalChanged().connect([this] {
        if (m_help)
            m_help->setMonoFamily(m_theme->monoFamily());
        updateLogColors();
    });
    updateLogColors();

    setState(State::Starting);
    m_backend.checkTools();
    focusPrimaryInput();
}

Window::~Window() = default;

// --- header and title bar ----------------------------------------------------------

void Window::buildHeader()
{
    auto *brand = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    auto *icon = Gtk::make_managed<Gtk::Image>(appIcon());
    icon->set_pixel_size(28);
    icon->add_css_class("brand-icon");
    brand->append(*icon);
    auto *wordmark = Gtk::make_managed<Gtk::Label>("LGL DNF HELPER");
    wordmark->add_css_class("wordmark");
    brand->append(*wordmark);
    brand->set_margin_end(12);
    m_header.pack_start(*brand);
    // The wordmark is the title; keep the centre empty.
    m_header.set_title_widget(*Gtk::make_managed<Gtk::Box>());

    const struct {
        const char *label;
        const char *action;
        const char *tooltip;
    } buttons[] = {
        {"Reset", "win.reset", "Clear the current search, package view, and log"},
    };
    for (const auto &spec : buttons) {
        auto *button = Gtk::make_managed<Gtk::Button>(spec.label);
        button->set_action_name(spec.action);
        button->set_tooltip_text(spec.tooltip);
        m_header.pack_start(*button);
    }

    auto menu = Gio::Menu::create();
    auto log = Gio::Menu::create();
    log->append("Clear Log", "win.clear-log");
    menu->append_section(log);
    auto view = Gio::Menu::create();
    view->append("Use System Title Bar", "win.system-title-bar");
    menu->append_section(view);
    auto help = Gio::Menu::create();
    help->append("Help", "win.help");
    help->append("About LGL DNF Helper", "win.about");
    menu->append_section(help);
    auto quit = Gio::Menu::create();
    quit->append("Quit", "app.quit");
    menu->append_section(quit);
    m_menuButton = Gtk::make_managed<Gtk::MenuButton>();
    m_menuButton->set_icon_name("open-menu-symbolic");
    m_menuButton->set_menu_model(menu);
    m_menuButton->set_tooltip_text("Main menu");
    m_menuButton->set_primary(true);
    m_header.pack_end(*m_menuButton);

    m_status.add_css_class("dh-chip");
    m_status.set_valign(Gtk::Align::CENTER);
    m_status.set_tooltip_text("Backend status");
    m_header.pack_end(m_status);
}

// The header bar is either the window's title bar (client-side decorations, with the window
// buttons) or, when the compositor draws the title bar, the first row of the window.
// Switching between the two needs the window unrealized, so it is briefly hidden. gtkmm
// removes a hidden window from its application, which would quit; the application is held
// and the window re-added.
void Window::applyTitleBar(const bool system)
{
    const bool realized = get_realized();
    const Glib::RefPtr<Gtk::Application> application = get_application();
    if (realized) {
        application->hold();
        set_visible(false);
        Gtk::Widget::unrealize();
    }
    if (system) {
        unset_titlebar();
        m_header.set_show_title_buttons(false);
        m_content.prepend(m_header);
    } else {
        if (m_header.get_parent())
            m_content.remove(m_header);
        m_header.set_show_title_buttons(true);
        set_titlebar(m_header);
    }
    if (realized) {
        set_application(application);
        present();
        focusPrimaryInput();
        application->release();
    }
}

void Window::focusPrimaryInput()
{
    m_search.grab_focus();
}

// --- body --------------------------------------------------------------------------

void Window::buildBody()
{
    auto *body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    body->add_css_class("body");

    auto *searchRow = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    m_search.set_placeholder_text("Search package, capability, or file path");
    m_search.set_hexpand(true);
    m_search.signal_activate().connect([this] { activate_action("win.search"); });
    searchRow->append(m_search);
    auto *search = Gtk::make_managed<Gtk::Button>("Search");
    search->set_action_name("win.search");
    search->add_css_class("dh-primary");
    search->add_css_class("suggested-action");
    search->set_tooltip_text("Search installed and available packages (Enter)");
    searchRow->append(*search);
    body->append(*searchRow);

    auto *split = Gtk::make_managed<Gtk::Paned>(Gtk::Orientation::HORIZONTAL);
    split->add_css_class("body-paned");
    split->set_start_child(buildResults());
    split->set_end_child(buildDetails());
    split->set_resize_start_child(false);
    split->set_shrink_start_child(false);
    split->set_shrink_end_child(false);
    split->set_position(320);
    split->set_vexpand(true);
    body->append(*split);

    m_notice.set_wrap(true);
    m_notice.set_max_width_chars(80);
    m_noticeBox.add_css_class("dh-notice");
    m_noticeBox.append(m_notice);
    m_noticeRevealer.set_child(m_noticeBox);
    m_noticeRevealer.set_transition_type(Gtk::RevealerTransitionType::SLIDE_UP);
    m_noticeRevealer.set_halign(Gtk::Align::CENTER);
    m_noticeRevealer.set_valign(Gtk::Align::END);
    m_noticeRevealer.set_can_target(false);

    auto *overlay = Gtk::make_managed<Gtk::Overlay>();
    overlay->set_child(*body);
    overlay->add_overlay(m_noticeRevealer);
    overlay->set_vexpand(true);
    m_content.append(*overlay);
    set_child(m_content);
}

Gtk::Widget &Window::buildResults()
{
    m_installedResults.setEmptyText("Installed matches appear here.");
    m_allResults.setEmptyText("Search for a package to begin.");

    auto *split = Gtk::make_managed<Gtk::Paned>(Gtk::Orientation::VERTICAL);
    split->add_css_class("body-paned");
    split->set_start_child(flushCard("INSTALLED PACKAGES", m_installedResults, &m_installedCount));
    split->set_end_child(flushCard("ALL PACKAGES", m_allResults, &m_allCount));
    split->set_shrink_start_child(false);
    split->set_shrink_end_child(false);
    split->set_position(260);
    split->set_size_request(280, -1);

    for (DataTable *table : {&m_installedResults, &m_allResults}) {
        table->signalClicked().connect([this](const Glib::RefPtr<TableRow> &row) {
            const std::string target = resultTarget(row);
            if (!target.empty())
                openPackage(target);
        });
        table->signalActivated().connect([this](const Glib::RefPtr<TableRow> &row) {
            const std::string target = resultTarget(row);
            if (!target.empty())
                openPackage(target);
        });
    }
    return *split;
}

Gtk::Widget &Window::buildDetails()
{
    auto *details = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);

    auto *packageCard = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    packageCard->add_css_class("dh-card");
    packageCard->append(sectionLabel("SELECTED PACKAGE"));
    m_packageTitle.add_css_class("package-title");
    m_packageTitle.set_xalign(0.0f);
    m_packageTitle.set_selectable(true);
    m_packageTitle.set_ellipsize(Pango::EllipsizeMode::END);
    m_packageNevra.add_css_class("mono");
    m_packageNevra.add_css_class("muted");
    m_packageNevra.set_xalign(0.0f);
    m_packageNevra.set_selectable(true);
    m_packageNevra.set_ellipsize(Pango::EllipsizeMode::END);
    packageCard->append(m_packageTitle);
    packageCard->append(m_packageNevra);
    packageCard->append(m_badges);
    details->append(*packageCard);
    clearPackageHeader("Search for an installed package to begin.");

    // Overview: package fields over a resizable description.
    m_description.set_editable(false);
    m_description.set_cursor_visible(false);
    m_description.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    m_description.add_css_class("description-view");
    m_description.set_left_margin(14);
    m_description.set_right_margin(14);
    m_description.set_top_margin(4);
    m_description.set_bottom_margin(12);
    auto *descriptionScroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    descriptionScroller->set_child(m_description);
    descriptionScroller->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    descriptionScroller->set_size_request(-1, 90);
    auto *overviewSplit = Gtk::make_managed<Gtk::Paned>(Gtk::Orientation::VERTICAL);
    overviewSplit->add_css_class("body-paned");
    overviewSplit->set_start_child(m_overview);
    overviewSplit->set_end_child(flushCard("DESCRIPTION", *descriptionScroller));
    overviewSplit->set_resize_end_child(false);
    overviewSplit->set_shrink_start_child(false);
    overviewSplit->set_shrink_end_child(false);
    overviewSplit->set_position(360);
    m_overviewPage = overviewSplit;

    m_log.set_editable(false);
    m_log.set_cursor_visible(false);
    m_log.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    m_log.set_monospace(true);
    m_log.add_css_class("log-view");
    m_log.set_left_margin(10);
    m_log.set_right_margin(10);
    m_log.set_top_margin(8);
    m_log.set_bottom_margin(8);
    for (const char *tag : {"positive", "neutral", "negative"})
        m_log.get_buffer()->create_tag(tag);
    auto *logScroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    logScroller->set_child(m_log);

    m_dependencies.set_tooltip_text("Double-click a row to open the package. Right-click to copy.");
    m_files.setEmptyText("RPM lists files for installed packages only.");
    m_requiredBy.set_tooltip_text("Double-click a row to open the package. Right-click to copy.");

    const struct {
        Gtk::Widget *page;
        const char *title;
    } pages[] = {
        {m_overviewPage, "Overview"},
        {&m_dependencies, "Dependencies"},
        {&m_requiredBy, "Required By"},
        {&m_files, "Files"},
        {&m_systemConfig, "System Config"},
        {&m_userConfig, "User Config"},
        {&m_historyTable, "History"},
        {&m_repository, "Repository"},
        {&m_impact, "Impact"},
        {logScroller, "Log"},
    };
    for (const auto &page : pages)
        m_tabs.append_page(*page.page, page.title);
    m_tabPages = {
        {&m_dependencies, Tab::Dependencies}, {&m_requiredBy, Tab::RequiredBy},
        {&m_files, Tab::Files}, {&m_systemConfig, Tab::SystemConfig},
        {&m_userConfig, Tab::UserConfig}, {&m_historyTable, Tab::History},
        {&m_repository, Tab::Repository}, {&m_impact, Tab::Impact},
    };
    m_tabs.set_scrollable(true);
    m_tabs.set_vexpand(true);
    m_tabs.add_css_class("detail-tabs");
    m_tabs.signal_switch_page().connect([this](Gtk::Widget *, guint) {
        // After the switch, so the new page is current.
        Glib::signal_idle().connect_once([this] { loadCurrentTab(); });
    });
    details->append(m_tabs);
    buildSetupPage();

    for (DataTable *table : {&m_installedResults, &m_allResults, &m_overview, &m_dependencies,
                             &m_requiredBy, &m_files, &m_systemConfig, &m_userConfig,
                             &m_historyTable, &m_repository, &m_impact})
        table->signalNotice().connect([this](const std::string &text, const bool failure) {
            notify(text, failure ? Tone::Negative : Tone::Plain, routineNoticeMs);
        });

    m_dependencies.signalActivated().connect([this](const Glib::RefPtr<TableRow> &row) {
        navigateFromDependency(row->cells, false);
    });
    m_requiredBy.signalActivated().connect([this](const Glib::RefPtr<TableRow> &row) {
        navigateFromDependency(row->cells, true);
    });
    m_overview.signalActivated().connect(
        [this](const Glib::RefPtr<TableRow> &) { m_overview.openSelectedUrl(); });
    return *details;
}

void Window::buildSetupPage()
{
    m_setupPage.add_css_class("dialog-body");
    m_setupPage.set_valign(Gtk::Align::CENTER);
    auto *title = Gtk::make_managed<Gtk::Label>("DNF5 not detected");
    title->add_css_class("setup-title");
    title->set_xalign(0.0f);
    auto *body = Gtk::make_managed<Gtk::Label>();
    body->set_markup(
        "LGL DNF Helper needs <b>dnf5</b> for dependency and repository queries.\n\n"
        "Install the required tools on Fedora with:\n"
        "<tt>sudo dnf install dnf5 rpm</tt>\n\n"
        "The app runs read-only package inspection as your regular user. "
        "Do not launch it with sudo.");
    body->set_wrap(true);
    body->set_xalign(0.0f);
    body->set_selectable(true);
    m_setupPage.append(*title);
    m_setupPage.append(*body);
}

// --- actions -----------------------------------------------------------------------

void Window::buildActions(const Glib::RefPtr<Gtk::Application> &application)
{
    m_searchAction = add_action("search", sigc::mem_fun(*this, &Window::runSearch));
    add_action("reset", sigc::mem_fun(*this, &Window::resetUi));
    add_action("focus-search", sigc::mem_fun(*this, &Window::focusPrimaryInput));
    add_action("clear-log", [this] { m_log.get_buffer()->set_text(""); });
    add_action("help", sigc::mem_fun(*this, &Window::showHelp));
    add_action("about", sigc::mem_fun(*this, &Window::showAbout));
    auto titleBar = add_action_bool("system-title-bar", false);
    titleBar->signal_activate().connect([titleBar](const Glib::VariantBase &) {
        bool system = false;
        titleBar->get_state(system);
        titleBar->change_state(!system);
    });
    titleBar->signal_change_state().connect([this, titleBar](const Glib::VariantBase &value) {
        const bool system = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(value).get();
        titleBar->set_state(value);
        m_settings.setSystemTitleBar(system);
        applyTitleBar(system);
    });
    titleBar->set_state(Glib::Variant<bool>::create(m_settings.systemTitleBar()));

    application->set_accel_for_action("win.focus-search", "<Control>f");
    application->set_accel_for_action("win.help", "F1");
    application->set_accel_for_action("win.system-title-bar", "<Control><Shift>m");
    application->set_accel_for_action("app.quit", "<Control>q");
}

// Alt pressed and released on its own opens or closes the main menu. F10 does the same,
// except Shift+F10 remains available for a selected table row's context menu.
void Window::watchAltTap(Gtk::Widget &widget)
{
    auto keys = Gtk::EventControllerKey::create();
    keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    keys->signal_key_pressed().connect(
        [this](guint keyval, guint, Gdk::ModifierType state) {
            const bool shiftHeld =
                (static_cast<guint>(state) & static_cast<guint>(Gdk::ModifierType::SHIFT_MASK)) != 0;
            if (keyval == GDK_KEY_F10 && !shiftHeld) {
                toggleMainMenu();
                return true;
            }
            m_altTapped = isAlt(keyval);
            return false;
        },
        false);
    keys->signal_key_released().connect([this](guint keyval, guint, Gdk::ModifierType) {
        if (m_altTapped && isAlt(keyval))
            toggleMainMenu();
        m_altTapped = false;
    });
    widget.add_controller(keys);
}

void Window::toggleMainMenu()
{
    if (m_menuButton->get_active()) {
        m_menuButton->popdown();
        // After the popover has handed focus back to the window.
        Glib::signal_idle().connect_once([this] { focusPrimaryInput(); });
        return;
    }
    m_menuButton->popup();
}

void Window::connectBackend()
{
    m_backend.toolsChecked.connect(sigc::mem_fun(*this, &Window::onToolsChecked));
    m_backend.searchCompleted.connect(sigc::mem_fun(*this, &Window::onSearchCompleted));
    m_backend.packageLoaded.connect([this](const Request &request, const Package &package) {
        if (!isCurrent(request))
            return;
        populatePackageHeader(package);
        populateOverview(package);
    });
    m_backend.dependenciesLoaded.connect(
        [this](const Request &request, const std::vector<DependencyEdge> &dependencies) {
            if (!isCurrent(request))
                return;
            populateDependencies(m_dependencies, dependencies);
            markTabLoaded(Tab::Dependencies);
        });
    m_backend.requiredByLoaded.connect(
        [this](const Request &request, const std::vector<DependencyEdge> &dependencies) {
            if (!isCurrent(request))
                return;
            std::vector<RowSpec> rows;
            for (const DependencyEdge &edge : dependencies)
                rows.push_back({{"Requires", edge.capability, edge.installReason, edge.repoId, ""}});
            m_requiredBy.setRows(rows);
            markTabLoaded(Tab::RequiredBy);
        });
    m_backend.filesLoaded.connect(
        [this](const Request &request, const std::vector<std::string> &files) {
            if (!isCurrent(request))
                return;
            std::vector<RowSpec> rows;
            rows.reserve(files.size());
            for (const std::string &path : files)
                rows.push_back({{fileType(path), path}});
            m_files.setRows(rows);
            markTabLoaded(Tab::Files);
        });
    const struct {
        sigc::signal<void(const Request &, const Rows &)> &signal;
        DataTable &table;
        Tab tab;
    } rowSignals[] = {
        {m_backend.systemConfigLoaded, m_systemConfig, Tab::SystemConfig},
        {m_backend.userConfigLoaded, m_userConfig, Tab::UserConfig},
        {m_backend.historyLoaded, m_historyTable, Tab::History},
        {m_backend.repositoryInfoLoaded, m_repository, Tab::Repository},
        {m_backend.impactLoaded, m_impact, Tab::Impact},
    };
    for (const auto &entry : rowSignals) {
        DataTable *table = &entry.table;
        const Tab tab = entry.tab;
        entry.signal.connect([this, table, tab](const Request &request, const Rows &rows) {
            if (!isCurrent(request))
                return;
            populateRows(*table, rows);
            markTabLoaded(tab);
        });
    }

    m_backend.commandStarted.connect([this](const std::string &command) {
        setState(State::Querying);
        appendLog("Running: " + command, Tone::Positive);
    });
    m_backend.commandFinished.connect([this](const std::string &summary, const bool ok) {
        appendLog(summary, ok ? Tone::Positive : Tone::Negative);
        if (!ok)
            m_loadingTabs.clear();
        setState(ok ? State::Ready : State::Error);
    });
    m_backend.errorOccurred.connect([this](const std::string &message) {
        appendLog(message, Tone::Negative);
        notify(message, Tone::Negative, routineNoticeMs);
    });
}

// --- notices, status and log -------------------------------------------------------

void Window::notify(const std::string &text, const Tone tone, const unsigned int timeoutMs)
{
    for (const char *name : {"positive", "neutral", "negative"})
        m_noticeBox.remove_css_class(name);
    if (const char *name = toneClass(static_cast<int>(tone)))
        m_noticeBox.add_css_class(name);
    m_notice.set_text(text);
    m_noticeRevealer.set_reveal_child(true);
    m_noticeTimer.disconnect();
    if (timeoutMs > 0)
        m_noticeTimer = Glib::signal_timeout().connect(
            [this] {
                m_noticeRevealer.set_reveal_child(false);
                return false;
            },
            timeoutMs);
}

void Window::setState(const State state)
{
    m_state = state;
    const char *text = "STARTING";
    Tone tone = Tone::Plain;
    switch (state) {
    case State::Starting: break;
    case State::Querying: text = "QUERYING"; tone = Tone::Neutral; break;
    case State::Ready: text = "READY"; tone = Tone::Positive; break;
    case State::Error: text = "ERROR"; tone = Tone::Negative; break;
    case State::SetupNeeded: text = "SETUP NEEDED"; tone = Tone::Neutral; break;
    }
    m_status.set_text(text);
    for (const char *name : {"positive", "neutral", "negative"})
        m_status.remove_css_class(name);
    if (const char *name = toneClass(static_cast<int>(tone)))
        m_status.add_css_class(name);
}

void Window::appendLog(const std::string &text, const Tone tone)
{
    auto buffer = m_log.get_buffer();
    const std::string line = "[" + Glib::DateTime::create_now_local().format("%H:%M:%S") + "] "
        + text + "\n";
    if (const char *name = toneClass(static_cast<int>(tone)))
        buffer->insert_with_tag(buffer->end(), line, name);
    else
        buffer->insert(buffer->end(), line);
    auto end = buffer->create_mark(buffer->end());
    m_log.scroll_to(end);
    buffer->delete_mark(end);
}

void Window::updateLogColors()
{
    const bool dark = m_theme && m_theme->current().dark;
    auto table = m_log.get_buffer()->get_tag_table();
    const std::pair<const char *, StatusColor> colors[] = {
        {"positive", positiveColor}, {"neutral", neutralColor}, {"negative", negativeColor}};
    for (const auto &[name, color] : colors) {
        if (auto tag = table->lookup(name))
            tag->property_foreground() = dark ? color.dark : color.light;
    }
}

// --- help and about ----------------------------------------------------------------

void Window::showHelp()
{
    if (!m_help)
        m_help = std::make_unique<HelpWindow>(*this);
    m_help->setMonoFamily(m_theme->monoFamily());
    m_help->present();
}

void Window::showAbout()
{
    if (!m_about) {
        m_about = std::make_unique<Gtk::AboutDialog>();
        m_about->set_transient_for(*this);
        m_about->set_modal(true);
        m_about->set_hide_on_close(true);
        m_about->set_program_name("LGL DNF Helper");
        m_about->set_version(DNFHELPER_VERSION);
        m_about->set_comments(
            "Understand the RPM packages on your Fedora system: what a package requires, what "
            "requires it, the files and config it owns, and where it came from. Read-only; it "
            "never installs, removes or updates packages.\n\nBuilt with GTK 4 on DNF5 and RPM.");
        m_about->set_website("https://github.com/linuxgamerlife/lgl-dnf-helper");
        m_about->set_website_label("Project page and issue tracker");
        m_about->set_authors({"LinuxGamerLife https://www.youtube.com/@linuxgamerlife"});
        m_about->add_credit_section("Support",
                                    {"Support development on Ko-fi https://ko-fi.com/G2G3V70LW"});
        m_about->set_license_type(Gtk::License::MIT_X11);
        m_about->set_logo(appIcon());
    }
    m_about->present();
}

// --- tools and setup mode ----------------------------------------------------------

void Window::onToolsChecked(const bool dnf5Available, const bool rpmAvailable)
{
    appendLog(std::string("Tool check: dnf5=") + (dnf5Available ? "yes" : "no")
              + " rpm=" + (rpmAvailable ? "yes" : "no"));
    if (!dnf5Available) {
        applySetupMode("dnf5 not found on PATH");
        return;
    }
    if (!rpmAvailable)
        appendLog("rpm not found: file ownership views will be limited", Tone::Neutral);
    applyNormalMode();
}

void Window::applySetupMode(const std::string &message)
{
    if (m_tabs.page_num(m_setupPage) < 0)
        m_tabs.insert_page(m_setupPage, "Setup", 0);
    m_tabs.set_current_page(0);
    m_searchAction->set_enabled(false);
    setState(State::SetupNeeded);
    notify(message, Tone::Neutral, readingNoticeMs);
}

void Window::applyNormalMode()
{
    const int setup = m_tabs.page_num(m_setupPage);
    if (setup >= 0)
        m_tabs.remove_page(setup);
    m_searchAction->set_enabled(true);
    setState(State::Ready);
}

// --- search and navigation ---------------------------------------------------------

void Window::runSearch()
{
    const std::string query = parsers::trimmed(m_search.get_text());
    if (query.empty()) {
        notify("Enter a package, capability, or file path", Tone::Plain, routineNoticeMs);
        return;
    }
    m_results.clear();
    m_currentPackage.clear();
    ++m_generation;
    m_loadedTabs.clear();
    m_loadingTabs.clear();
    m_installedResults.clear();
    m_allResults.clear();
    m_installedCount.set_text("");
    m_allCount.set_text("");
    m_overview.clear();
    clearPackageTables();
    m_description.get_buffer()->set_text("");
    clearPackageHeader("Select a package from the search results to view details.");
    m_resetToOverviewOnSearchResult = true;
    m_backend.search({query, ++m_searchGeneration});
}

void Window::resetUi()
{
    m_results.clear();
    m_currentPackage.clear();
    ++m_generation;
    ++m_searchGeneration;
    m_loadedTabs.clear();
    m_loadingTabs.clear();
    m_resetToOverviewOnSearchResult = false;

    m_search.set_text("");
    m_installedResults.clear();
    m_allResults.clear();
    m_installedCount.set_text("");
    m_allCount.set_text("");
    m_overview.clear();
    clearPackageTables();
    m_description.get_buffer()->set_text("");
    m_log.get_buffer()->set_text("");
    clearPackageHeader("Search for an installed package to begin.");
    if (const int overview = m_tabs.page_num(*m_overviewPage); overview >= 0)
        m_tabs.set_current_page(overview);
    setState(State::Ready);
    notify("Reset", Tone::Plain, routineNoticeMs);
    focusPrimaryInput();
}

void Window::onSearchCompleted(const Request &request,
                               const std::vector<PackageQueryResult> &results)
{
    if (request.generation != m_searchGeneration)
        return;
    m_results = results;
    std::vector<RowSpec> installed;
    std::vector<RowSpec> all;
    std::set<std::string> allKeys;
    for (std::size_t index = 0; index < results.size(); ++index) {
        const Package &package = results[index].package;
        const int data = static_cast<int>(index);
        if (package.installed)
            installed.push_back({{package.id.name}, "", data});
        // Installed and available copies of the same build are listed once.
        if (allKeys.insert(package.id.nevra()).second)
            all.push_back({{package.id.name, package.id.evr(), package.id.arch}, "", data});
    }
    m_installedResults.setRows(installed);
    m_allResults.setRows(all);
    m_installedCount.set_text(countText(m_installedResults.size()));
    m_allCount.set_text(countText(m_allResults.size()));
    notify(std::to_string(results.size()) + " result(s)", Tone::Plain, routineNoticeMs);

    if (m_resetToOverviewOnSearchResult) {
        if (const int overview = m_tabs.page_num(*m_overviewPage); overview >= 0)
            m_tabs.set_current_page(overview);
        m_resetToOverviewOnSearchResult = false;
    }
}

std::string Window::resultTarget(const Glib::RefPtr<TableRow> &row) const
{
    if (!row || row->data < 0 || row->data >= static_cast<int>(m_results.size()))
        return {};
    const Package &package = m_results[static_cast<std::size_t>(row->data)].package;
    // An available package is opened by its exact build.
    return package.installed ? package.id.name : package.id.nevra();
}

void Window::openPackage(const std::string &name)
{
    const std::string target = parsers::trimmed(name);
    // A double click follows the click that already opened it.
    if (target.empty() || target == m_currentPackage)
        return;
    m_currentPackage = target;
    ++m_generation;
    m_loadedTabs.clear();
    m_loadingTabs.clear();
    clearPackageTables();
    m_overview.clear();
    m_description.get_buffer()->set_text("");
    clearPackageHeader("Loading package details...");
    m_packageTitle.set_text(target);
    m_backend.loadPackage(currentRequest());
    loadCurrentTab();
}

void Window::navigateFromDependency(const Row &row, const bool requiredBy)
{
    const auto cell = [&row](const std::size_t index) {
        return index < row.size() && row[index] != "—" ? row[index] : std::string();
    };
    if (requiredBy) {
        openPackage(cell(1));
        return;
    }
    // The installed provider, else an available provider, else the package that provides the
    // capability (a file, soname or versioned name is not itself a package name).
    for (const std::size_t index : {2, 3}) {
        if (const std::string provider = cell(index); !provider.empty()) {
            openPackage(provider);
            return;
        }
    }
    const std::string capability = cell(1);
    if (capability.empty())
        return;
    const std::uint64_t generation = ++m_resolveGeneration;
    m_backend.resolveProvider(capability, [this, capability, generation](const std::string &provider) {
        if (generation != m_resolveGeneration)
            return;
        if (provider.empty())
            notify("No package provides " + capability, Tone::Neutral, routineNoticeMs);
        else
            openPackage(provider);
    });
}

// --- detail tabs -------------------------------------------------------------------

Request Window::currentRequest() const
{
    return {m_currentPackage, m_generation};
}

bool Window::isCurrent(const Request &request) const
{
    return request.generation == m_generation && request.name == m_currentPackage;
}

void Window::loadCurrentTab()
{
    if (m_currentPackage.empty())
        return;
    const auto found = m_tabPages.find(m_tabs.get_nth_page(m_tabs.get_current_page()));
    if (found == m_tabPages.end() || !shouldLoadTab(found->second))
        return;
    const Request request = currentRequest();
    switch (found->second) {
    case Tab::Dependencies: m_backend.loadDependencies(request); break;
    case Tab::RequiredBy: m_backend.loadRequiredBy(request); break;
    case Tab::Files: m_backend.loadFiles(request); break;
    case Tab::SystemConfig: m_backend.loadSystemConfig(request); break;
    case Tab::UserConfig: m_backend.loadUserConfig(request); break;
    case Tab::History: m_backend.loadHistory(request); break;
    case Tab::Repository: m_backend.loadRepositoryInfo(request); break;
    case Tab::Impact: m_backend.loadImpact(request); break;
    }
}

bool Window::shouldLoadTab(const Tab tab)
{
    if (m_loadedTabs.count(tab) != 0 || m_loadingTabs.count(tab) != 0)
        return false;
    m_loadingTabs.insert(tab);
    return true;
}

void Window::markTabLoaded(const Tab tab)
{
    m_loadingTabs.erase(tab);
    m_loadedTabs.insert(tab);
    if (m_state == State::Querying)
        setState(State::Ready);
}

void Window::clearPackageTables()
{
    for (DataTable *table : {&m_dependencies, &m_requiredBy, &m_files, &m_systemConfig,
                             &m_userConfig, &m_historyTable, &m_repository, &m_impact})
        table->clear();
}

void Window::clearPackageHeader(const std::string &hint)
{
    m_packageTitle.set_text("No package selected");
    m_packageNevra.set_text(hint);
    while (auto *child = m_badges.get_first_child())
        m_badges.remove(*child);
}

void Window::populatePackageHeader(const Package &package)
{
    m_packageTitle.set_text(package.id.name);
    m_packageNevra.set_text(package.id.nevra());
    while (auto *child = m_badges.get_first_child())
        m_badges.remove(*child);
    for (const Badge &badge : badgesForPackage(package)) {
        auto *chip = Gtk::make_managed<Gtk::Label>(badge.text);
        chip->add_css_class("dh-chip");
        if (const char *name = toneClass(static_cast<int>(badge.kind)))
            chip->add_css_class(name);
        m_badges.append(*chip);
    }
}

void Window::populateOverview(const Package &package)
{
    std::string installTime;
    if (package.installTime > 0)
        installTime = Glib::DateTime::create_now_local(package.installTime).format("%FT%T");
    const std::vector<RowSpec> rows = {
        {{"Name", package.id.name}},
        {{"Version", package.id.evr()}},
        {{"Architecture", package.id.arch}},
        {{"Summary", package.summary}},
        {{"Repository", package.repoId}},
        {{"Install reason", package.installReason}},
        {{"Installed size", package.installSize > 0 ? humanSize(package.installSize) : ""}},
        {{"Install time", installTime}},
        {{"License", package.license}},
        {{"URL", package.url}},
        {{"Source RPM", package.sourceRpm}},
        {{"Vendor", package.vendor}},
        {{"Packager", package.packager}},
    };
    m_overview.setRows(rows);
    m_description.get_buffer()->set_text(
        !package.description.empty() ? package.description
        : !package.summary.empty()   ? package.summary
                                     : "No description available.");
}

void Window::populateDependencies(DataTable &table, const std::vector<DependencyEdge> &dependencies)
{
    std::vector<RowSpec> rows;
    rows.reserve(dependencies.size());
    for (const DependencyEdge &edge : dependencies)
        rows.push_back({{dependencyTypeLabel(edge.type), edge.capability,
                         edge.installedProvider.name,
                         edge.availableProviders.empty() ? "" : edge.availableProviders.front().name,
                         ""},
                        edge.type == DependencyType::BuildRequires ? buildRequiresTooltip : ""});
    table.setRows(rows);
}

void Window::populateRows(DataTable &table, const Rows &rows)
{
    std::vector<RowSpec> specs;
    specs.reserve(rows.size());
    for (const Row &row : rows) {
        const bool leaf = &table == &m_impact && !row.empty() && row.front() == "Leaf package?";
        specs.push_back({row, leaf ? leafTooltip : ""});
    }
    table.setRows(specs);
}

} // namespace dnfhelper
