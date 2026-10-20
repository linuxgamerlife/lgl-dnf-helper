#pragma once

#include "core/package.h"

#include <giomm/liststore.h>
#include <giomm/simpleactiongroup.h>
#include <gtkmm/box.h>
#include <gtkmm/columnview.h>
#include <gtkmm/label.h>
#include <gtkmm/listitem.h>
#include <gtkmm/popovermenu.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/singleselection.h>
#include <gtkmm/sortlistmodel.h>
#include <gtkmm/stack.h>

#include <optional>
#include <string>
#include <vector>

namespace dnfhelper {

// One table row: its cells, a tooltip for the whole row and a value for the caller (the
// search result index of a result row).
class TableRow final : public Glib::Object {
public:
    static Glib::RefPtr<TableRow> create(Row cells, std::string tooltip, int data);

    Row cells;
    std::string tooltip;
    int data{-1};

private:
    TableRow(Row cells, std::string tooltip, int data);
};

struct RowSpec {
    RowSpec(Row cells_, std::string tooltip_ = {}, const int data_ = -1)
        : cells(std::move(cells_)), tooltip(std::move(tooltip_)), data(data_)
    {
    }

    Row cells;
    std::string tooltip;
    int data;
};

// A read-only, sortable table with resizable columns,
// single row selection, empty cells shown as "—", URLs styled as links, and a context menu
// (right click, Menu key or Shift+F10) to copy the cell, row or table and to open a URL or
// the folder of a local path.
class DataTable final : public Gtk::Box {
public:
    // wrap: cells of the expanding column wrap instead of being ellipsized. expandColumn: the
    // column that takes the spare width (the last one by default).
    explicit DataTable(std::vector<std::string> headers, bool wrap = false,
                       int expandColumn = -1);
    ~DataTable() override;

    void setRows(const std::vector<RowSpec> &rows);
    void clear();
    guint size() const { return m_store->get_n_items(); }

    // Shown in place of the table while it is empty; nothing when empty.
    void setEmptyText(const std::string &text);

    // The selected row's cells, in column order.
    std::optional<Row> selectedRow() const;
    // Tab-separated headers and rows, in the order shown.
    std::string tableText() const;

    void focusTable();
    // Opens the first URL in the selected row, if there is one.
    void openSelectedUrl();

    // A primary-button click on a row, and a row activated by double click or Enter.
    sigc::signal<void(const Glib::RefPtr<TableRow> &)> &signalClicked() { return m_clicked; }
    sigc::signal<void(const Glib::RefPtr<TableRow> &)> &signalActivated() { return m_activated; }
    // Text for an in-window notice, and whether it reports a failure.
    sigc::signal<void(const std::string &, bool)> &signalNotice() { return m_notice; }

private:
    void setupCell(const Glib::RefPtr<Gtk::ListItem> &listItem, int column);
    void bindCell(const Glib::RefPtr<Gtk::ListItem> &listItem, int column);
    // The row and column under a point of the view; position -1 when there is none.
    std::pair<int, int> cellAt(double x, double y);
    Glib::RefPtr<TableRow> rowAt(guint position) const;
    void showContextMenu(int position, int column, const Gdk::Rectangle &pointer);
    void updateEmptyState();

    void copyText(const std::string &text, const std::string &notice);
    void openContainingFolder();
    std::string selectedUrl() const;
    std::string selectedLocalPath() const;

    std::vector<std::string> m_headers;
    bool m_wrap;
    int m_expandColumn;
    Glib::RefPtr<Gio::ListStore<TableRow>> m_store;
    Glib::RefPtr<Gtk::SortListModel> m_sorted;
    Glib::RefPtr<Gtk::SingleSelection> m_selection;
    Gtk::ColumnView m_view;
    Gtk::ScrolledWindow m_scroller;
    Gtk::Label m_empty;
    Gtk::Stack m_stack;
    Gtk::PopoverMenu m_menu;
    Glib::RefPtr<Gio::SimpleActionGroup> m_actions;
    Glib::RefPtr<Gio::SimpleAction> m_openUrl;
    Glib::RefPtr<Gio::SimpleAction> m_openFolder;
    int m_menuColumn{0};
    std::string m_emptyText;

    sigc::signal<void(const Glib::RefPtr<TableRow> &)> m_clicked;
    sigc::signal<void(const Glib::RefPtr<TableRow> &)> m_activated;
    sigc::signal<void(const std::string &, bool)> m_notice;
};

} // namespace dnfhelper
