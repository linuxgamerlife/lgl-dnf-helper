#include "ui/datatable.h"

#include <gdk/gdkkeysyms.h>
#include <gdkmm/clipboard.h>
#include <giomm/file.h>
#include <giomm/menu.h>
#include <giomm/menuitem.h>
#include <gtkmm/columnviewcolumn.h>
#include <gtkmm/expression.h>
#include <gtkmm/filelauncher.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/shortcutcontroller.h>
#include <gtkmm/signallistitemfactory.h>
#include <gtkmm/stringsorter.h>
#include <gtkmm/urilauncher.h>
#include <gtkmm/window.h>

#include <filesystem>

namespace dnfhelper {

namespace {

const char *const emptyCell = "—";

bool isUrl(const std::string &text)
{
    return text.rfind("http://", 0) == 0 || text.rfind("https://", 0) == 0;
}

// The label in one cell. Remembers its list item, which owns it, and its column, so a point
// in the view can be mapped to a row and column.
class CellLabel final : public Gtk::Label {
public:
    CellLabel(GtkListItem *listItem, const int column)
        : m_listItem(listItem)
        , m_column(column)
    {
    }

    int position() const
    {
        const guint position = gtk_list_item_get_position(m_listItem);
        return position == GTK_INVALID_LIST_POSITION ? -1 : static_cast<int>(position);
    }
    int column() const { return m_column; }

private:
    GtkListItem *m_listItem;
    int m_column;
};

} // namespace

Glib::RefPtr<TableRow> TableRow::create(Row cells, std::string tooltip, const int data)
{
    return Glib::make_refptr_for_instance(new TableRow(std::move(cells), std::move(tooltip), data));
}

TableRow::TableRow(Row cells_, std::string tooltip_, const int data_)
    : Glib::ObjectBase(typeid(TableRow))
    , cells(std::move(cells_))
    , tooltip(std::move(tooltip_))
    , data(data_)
{
}

DataTable::DataTable(std::vector<std::string> headers, const bool wrap, const int expandColumn)
    : Gtk::Box(Gtk::Orientation::VERTICAL)
    , m_headers(std::move(headers))
    , m_wrap(wrap)
    , m_expandColumn(expandColumn < 0 ? static_cast<int>(m_headers.size()) - 1 : expandColumn)
    , m_store(Gio::ListStore<TableRow>::create())
    , m_actions(Gio::SimpleActionGroup::create())
{
    m_sorted = Gtk::SortListModel::create(m_store, m_view.get_sorter());
    m_selection = Gtk::SingleSelection::create(m_sorted);
    m_selection->set_autoselect(false);
    m_selection->set_can_unselect(true);
    m_view.set_model(m_selection);
    m_view.add_css_class("data-table");
    m_view.set_reorderable(false);

    for (int column = 0; column < static_cast<int>(m_headers.size()); ++column) {
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect(
            [this, column](const Glib::RefPtr<Gtk::ListItem> &item) { setupCell(item, column); });
        factory->signal_bind().connect(
            [this, column](const Glib::RefPtr<Gtk::ListItem> &item) { bindCell(item, column); });
        auto viewColumn = Gtk::ColumnViewColumn::create(m_headers[static_cast<std::size_t>(column)],
                                                        factory);
        viewColumn->set_resizable(true);
        viewColumn->set_expand(column == m_expandColumn);
        auto text = Gtk::ClosureExpression<Glib::ustring>::create(
            [column](const Glib::RefPtr<Glib::ObjectBase> &object) -> Glib::ustring {
                const auto row = std::dynamic_pointer_cast<TableRow>(object);
                const auto index = static_cast<std::size_t>(column);
                return row && index < row->cells.size() ? row->cells[index] : "";
            });
        viewColumn->set_sorter(Gtk::StringSorter::create(text));
        m_view.append_column(viewColumn);
    }

    m_view.signal_activate().connect([this](const guint position) {
        if (auto row = rowAt(position))
            m_activated.emit(row);
    });

    // Capture phase: runs before the row's own handling, and finds the row under the pointer
    // for a primary click and for the context menu.
    auto click = Gtk::GestureClick::create();
    click->set_button(0);
    click->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    click->signal_pressed().connect([this, click](const int presses, const double x,
                                                  const double y) {
        const auto [position, column] = cellAt(x, y);
        if (position < 0)
            return;
        const guint button = click->get_current_button();
        if (button == GDK_BUTTON_SECONDARY) {
            click->set_state(Gtk::EventSequenceState::CLAIMED);
            showContextMenu(position, column,
                            Gdk::Rectangle(static_cast<int>(x), static_cast<int>(y), 1, 1));
        } else if (button == GDK_BUTTON_PRIMARY && presses == 1) {
            if (auto row = rowAt(static_cast<guint>(position)))
                m_clicked.emit(row);
        }
    });
    m_view.add_controller(click);

    const auto keyboardMenu = Gtk::CallbackAction::create(
        [this](Gtk::Widget &, const Glib::VariantBase &) {
            const guint selected = m_selection->get_selected();
            if (selected == GTK_INVALID_LIST_POSITION)
                return false;
            // Point at the focused row when there is one.
            Gdk::Rectangle pointer(m_view.get_width() / 2, 24, 1, 1);
            if (auto *root = m_view.get_root()) {
                if (auto *focus = root->get_focus()) {
                    if (const auto bounds = focus->compute_bounds(m_view))
                        pointer = Gdk::Rectangle(static_cast<int>(bounds->get_x()) + 24,
                                                 static_cast<int>(bounds->get_y()
                                                                  + bounds->get_height()),
                                                 1, 1);
                }
            }
            showContextMenu(static_cast<int>(selected), 0, pointer);
            return true;
        });
    auto shortcuts = Gtk::ShortcutController::create();
    shortcuts->add_shortcut(
        Gtk::Shortcut::create(Gtk::KeyvalTrigger::create(GDK_KEY_Menu), keyboardMenu));
    shortcuts->add_shortcut(Gtk::Shortcut::create(
        Gtk::KeyvalTrigger::create(GDK_KEY_F10, Gdk::ModifierType::SHIFT_MASK), keyboardMenu));
    m_view.add_controller(shortcuts);

    m_actions->add_action("copy-cell", [this] {
        const auto row = selectedRow();
        const auto index = static_cast<std::size_t>(m_menuColumn);
        copyText(row && index < row->size() ? (*row)[index] : std::string(), "Copied cell");
    });
    m_actions->add_action("copy-row", [this] {
        std::string text;
        if (const auto row = selectedRow()) {
            for (std::size_t i = 0; i < row->size(); ++i)
                text += (i == 0 ? "" : "\t") + (*row)[i];
        }
        copyText(text, "Copied row");
    });
    m_actions->add_action("copy-table", [this] { copyText(tableText(), "Copied table"); });
    m_openUrl = m_actions->add_action("open-url", sigc::mem_fun(*this, &DataTable::openSelectedUrl));
    m_openFolder = m_actions->add_action("open-folder",
                                         sigc::mem_fun(*this, &DataTable::openContainingFolder));
    insert_action_group("table", m_actions);

    auto menu = Gio::Menu::create();
    auto copy = Gio::Menu::create();
    copy->append("Copy Cell", "table.copy-cell");
    copy->append("Copy Row", "table.copy-row");
    copy->append("Copy Table", "table.copy-table");
    menu->append_section(copy);
    auto open = Gio::Menu::create();
    for (const auto &[label, action] : {std::pair{"Open URL", "table.open-url"},
                                        std::pair{"Open Containing Folder", "table.open-folder"}}) {
        auto item = Gio::MenuItem::create(label, action);
        // Offered only for rows that have a URL or a local path.
        item->set_attribute_value("hidden-when",
                                  Glib::Variant<Glib::ustring>::create("action-disabled"));
        open->append_item(item);
    }
    menu->append_section(open);
    m_menu.set_menu_model(menu);
    m_menu.set_has_arrow(false);
    m_menu.set_halign(Gtk::Align::START);
    m_menu.set_parent(m_view);

    m_scroller.set_child(m_view);
    m_scroller.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_scroller.set_vexpand(true);
    m_scroller.set_hexpand(true);

    m_empty.add_css_class("muted");
    m_empty.add_css_class("table-empty");
    m_empty.set_wrap(true);
    m_empty.set_justify(Gtk::Justification::CENTER);
    m_stack.add(m_empty, "empty");
    m_stack.add(m_scroller, "table");
    m_stack.set_vexpand(true);
    m_stack.set_visible_child("table");
    append(m_stack);
}

DataTable::~DataTable()
{
    m_menu.unparent();
}

void DataTable::setupCell(const Glib::RefPtr<Gtk::ListItem> &listItem, const int column)
{
    auto *label = Gtk::make_managed<CellLabel>(listItem->gobj(), column);
    label->set_xalign(0.0f);
    label->set_valign(Gtk::Align::START);
    if (m_wrap && column == m_expandColumn) {
        label->set_wrap(true);
        label->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
        // Wrap to the column instead of widening it.
        label->set_width_chars(1);
        label->set_max_width_chars(1);
        label->set_hexpand(true);
    } else {
        label->set_ellipsize(Pango::EllipsizeMode::END);
    }
    listItem->set_child(*label);
}

void DataTable::bindCell(const Glib::RefPtr<Gtk::ListItem> &listItem, const int column)
{
    auto *label = dynamic_cast<Gtk::Label *>(listItem->get_child());
    const auto row = std::dynamic_pointer_cast<TableRow>(listItem->get_item());
    if (!label || !row)
        return;
    const auto index = static_cast<std::size_t>(column);
    const std::string text = index < row->cells.size() ? row->cells[index] : emptyCell;
    label->set_text(text);
    if (isUrl(text)) {
        label->add_css_class("link-cell");
        label->set_tooltip_text("Double-click or right-click to open this URL");
    } else {
        label->remove_css_class("link-cell");
        if (!row->tooltip.empty())
            label->set_tooltip_text(row->tooltip);
        else if (!m_wrap && text.size() > 40)
            // Long values are ellipsized; show them in full on hover.
            label->set_tooltip_text(text);
        else
            label->set_has_tooltip(false);
    }
}

void DataTable::setRows(const std::vector<RowSpec> &rows)
{
    std::vector<Glib::RefPtr<TableRow>> items;
    items.reserve(rows.size());
    for (const RowSpec &spec : rows) {
        Row cells = spec.cells;
        cells.resize(m_headers.size());
        for (std::string &cell : cells) {
            if (cell.empty())
                cell = emptyCell;
        }
        items.push_back(TableRow::create(std::move(cells), spec.tooltip, spec.data));
    }
    m_selection->set_selected(GTK_INVALID_LIST_POSITION);
    m_store->splice(0, m_store->get_n_items(), items);
    updateEmptyState();
}

void DataTable::clear()
{
    m_selection->set_selected(GTK_INVALID_LIST_POSITION);
    m_store->remove_all();
    updateEmptyState();
}

void DataTable::setEmptyText(const std::string &text)
{
    m_emptyText = text;
    m_empty.set_text(text);
    updateEmptyState();
}

void DataTable::updateEmptyState()
{
    m_stack.set_visible_child(size() == 0 && !m_emptyText.empty() ? "empty" : "table");
}

void DataTable::focusTable()
{
    m_view.grab_focus();
}

Glib::RefPtr<TableRow> DataTable::rowAt(const guint position) const
{
    return std::dynamic_pointer_cast<TableRow>(m_sorted->get_object(position));
}

std::pair<int, int> DataTable::cellAt(const double x, const double y)
{
    for (Gtk::Widget *widget = m_view.pick(x, y); widget && widget != &m_view;
         widget = widget->get_parent()) {
        if (auto *cell = dynamic_cast<CellLabel *>(widget))
            return {cell->position(), cell->column()};
        // The padding around a cell picks the cell, not its label.
        if (auto *cell = dynamic_cast<CellLabel *>(widget->get_first_child()))
            return {cell->position(), cell->column()};
    }
    return {-1, 0};
}

std::optional<Row> DataTable::selectedRow() const
{
    const guint selected = m_selection->get_selected();
    if (selected == GTK_INVALID_LIST_POSITION)
        return std::nullopt;
    if (const auto row = rowAt(selected))
        return row->cells;
    return std::nullopt;
}

std::string DataTable::tableText() const
{
    std::string text;
    for (std::size_t i = 0; i < m_headers.size(); ++i)
        text += (i == 0 ? "" : "\t") + m_headers[i];
    for (guint position = 0; position < m_sorted->get_n_items(); ++position) {
        const auto row = rowAt(position);
        if (!row)
            continue;
        text += '\n';
        for (std::size_t i = 0; i < row->cells.size(); ++i)
            text += (i == 0 ? "" : "\t") + row->cells[i];
    }
    return text;
}

void DataTable::showContextMenu(const int position, const int column,
                                const Gdk::Rectangle &pointer)
{
    m_selection->set_selected(static_cast<guint>(position));
    m_menuColumn = column;
    m_openUrl->set_enabled(!selectedUrl().empty());
    m_openFolder->set_enabled(!selectedLocalPath().empty());
    m_menu.set_pointing_to(pointer);
    m_menu.popup();
}

void DataTable::copyText(const std::string &text, const std::string &notice)
{
    get_clipboard()->set_text(text);
    m_notice.emit(notice, false);
}

std::string DataTable::selectedUrl() const
{
    if (const auto row = selectedRow()) {
        for (const std::string &cell : *row) {
            if (isUrl(cell))
                return cell;
        }
    }
    return {};
}

std::string DataTable::selectedLocalPath() const
{
    if (const auto row = selectedRow()) {
        for (const std::string &cell : *row) {
            if (cell.size() > 1 && cell.front() == '/')
                return cell;
        }
    }
    return {};
}

void DataTable::openSelectedUrl()
{
    const std::string url = selectedUrl();
    auto *window = dynamic_cast<Gtk::Window *>(get_root());
    if (url.empty() || !window)
        return;
    auto launcher = Gtk::UriLauncher::create(url);
    launcher->launch(*window, [this, launcher](const Glib::RefPtr<Gio::AsyncResult> &result) {
        try {
            launcher->launch_finish(result);
        } catch (const Glib::Error &) {
            m_notice.emit("Could not open the URL", true);
        }
    });
}

void DataTable::openContainingFolder()
{
    const std::string path = selectedLocalPath();
    auto *window = dynamic_cast<Gtk::Window *>(get_root());
    if (path.empty() || !window)
        return;
    std::error_code error;
    const bool directory = std::filesystem::is_directory(path, error);
    auto launcher = Gtk::FileLauncher::create(Gio::File::create_for_path(path));
    const auto finished = [this, launcher, directory](const Glib::RefPtr<Gio::AsyncResult> &result) {
        try {
            if (directory)
                launcher->launch_finish(result);
            else
                launcher->open_containing_folder_finish(result);
        } catch (const Glib::Error &) {
            m_notice.emit("Could not open containing folder", true);
        }
    };
    // A folder opens itself; a file opens its folder with the file selected.
    if (directory)
        launcher->launch(*window, finished);
    else
        launcher->open_containing_folder(*window, finished);
}

} // namespace dnfhelper
