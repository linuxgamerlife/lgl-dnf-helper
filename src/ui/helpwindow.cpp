#include "ui/helpwindow.h"

#include <gdk/gdkkeysyms.h>
#include <giomm/resource.h>
#include <glib.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/grid.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textchildanchor.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>
#include <vector>

namespace dnfhelper {

namespace {

using Tags = std::vector<Glib::RefPtr<Gtk::TextTag>>;

Glib::RefPtr<Gtk::TextTag> tag(const Glib::RefPtr<Gtk::TextBuffer> &buffer, const char *name)
{
    return buffer->get_tag_table()->lookup(name);
}

struct HtmlRenderer {
    struct Element {
        std::string name;
        Tags tags;
        bool chip{false};
        std::string chipClasses;
        std::string chipText;
    };

    struct List {
        bool ordered{false};
        unsigned int next{1};
    };

    struct Table {
        struct Row {
            std::vector<std::string> cells;
            bool heading{false};
        };
        std::vector<Row> rows;
        Row current;
    };

    Gtk::TextView &view;
    Glib::RefPtr<Gtk::TextBuffer> buffer;
    std::vector<Element> elements;
    std::vector<List> lists;
    std::optional<Table> table;

    explicit HtmlRenderer(Gtk::TextView &textView)
        : view(textView)
        , buffer(textView.get_buffer())
    {
    }

    static void start(GMarkupParseContext *, const gchar *name, const gchar **attributeNames,
                      const gchar **attributeValues, gpointer data, GError **)
    {
        static_cast<HtmlRenderer *>(data)->startElement(name, attributeNames, attributeValues);
    }

    static void end(GMarkupParseContext *, const gchar *, gpointer data, GError **)
    {
        static_cast<HtmlRenderer *>(data)->endElement();
    }

    static void text(GMarkupParseContext *, const gchar *text, gsize length, gpointer data,
                     GError **)
    {
        static_cast<HtmlRenderer *>(data)->appendText(std::string(text, length));
    }

    static std::string attribute(const gchar **names, const gchar **values, const std::string_view wanted)
    {
        if (!names || !values)
            return {};
        for (std::size_t i = 0; names[i]; ++i) {
            if (wanted == names[i])
                return values[i];
        }
        return {};
    }

    static bool hasClass(const std::string &classes, const std::string_view wanted)
    {
        std::size_t start = 0;
        while (start < classes.size()) {
            const auto end = classes.find(' ', start);
            if (classes.substr(start, end - start) == wanted)
                return true;
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
        return false;
    }

    void ensureLineBreak()
    {
        auto end = buffer->end();
        if (end.get_offset() == 0)
            return;
        auto previous = end;
        previous.backward_char();
        if (previous.get_char() != '\n')
            buffer->insert(end, "\n");
    }

    void append(const std::string &text, Tags tags = {})
    {
        if (text.empty())
            return;
        if (tags.empty()) {
            for (const Element &element : elements)
                tags.insert(tags.end(), element.tags.begin(), element.tags.end());
        }
        if (tags.empty())
            buffer->insert(buffer->end(), text);
        else
            buffer->insert_with_tags(buffer->end(), text, tags);
    }

    void appendChip(const std::string &classes, const std::string &text)
    {
        auto *chip = Gtk::make_managed<Gtk::Label>(text);
        chip->add_css_class("dh-chip");
        std::size_t start = 0;
        while (start < classes.size()) {
            const auto end = classes.find(' ', start);
            const auto name = classes.substr(start, end - start);
            if (!name.empty() && name != "chip")
                chip->add_css_class(name);
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
        view.add_child_at_anchor(*chip, buffer->create_child_anchor(buffer->end()));
    }

    static std::string collapseWhitespace(const std::string &text)
    {
        std::string result;
        bool whitespace = false;
        for (const unsigned char character : text) {
            if (std::isspace(character)) {
                whitespace = true;
            } else {
                if (whitespace && !result.empty())
                    result += ' ';
                result += static_cast<char>(character);
                whitespace = false;
            }
        }
        return result;
    }

    bool inTableCell() const
    {
        return table && !table->current.cells.empty()
               && !elements.empty()
               && (elements.back().name == "td" || elements.back().name == "th"
                   || elements.back().name == "strong" || elements.back().name == "code");
    }

    void renderTable()
    {
        if (!table)
            return;
        auto *grid = Gtk::make_managed<Gtk::Grid>();
        grid->add_css_class("help-table");
        grid->set_column_spacing(0);
        grid->set_row_spacing(0);
        for (std::size_t rowIndex = 0; rowIndex < table->rows.size(); ++rowIndex) {
            const Table::Row &row = table->rows[rowIndex];
            for (std::size_t columnIndex = 0; columnIndex < row.cells.size(); ++columnIndex) {
                auto *cell = Gtk::make_managed<Gtk::Label>(row.cells[columnIndex]);
                cell->add_css_class("help-table-cell");
                if (row.heading)
                    cell->add_css_class("help-table-heading");
                if (columnIndex == 0)
                    cell->add_css_class("help-table-first-column");
                cell->set_xalign(0.0f);
                cell->set_yalign(0.0f);
                cell->set_wrap(true);
                cell->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
                if (columnIndex == 0) {
                    cell->set_width_chars(18);
                } else {
                    cell->set_hexpand(true);
                    cell->set_width_chars(48);
                    cell->set_max_width_chars(64);
                }
                grid->attach(*cell, static_cast<int>(columnIndex), static_cast<int>(rowIndex), 1, 1);
            }
        }
        view.add_child_at_anchor(*grid, buffer->create_child_anchor(buffer->end()));
        buffer->insert(buffer->end(), "\n");
        table.reset();
    }

    void startElement(const std::string &name, const gchar **attributeNames,
                      const gchar **attributeValues)
    {
        Element element;
        element.name = name;
        if (name == "table") {
            ensureLineBreak();
            table.emplace();
        } else if (name == "tr" && table) {
            table->current = {};
        } else if ((name == "td" || name == "th") && table) {
            table->current.cells.emplace_back();
            table->current.heading = table->current.heading || name == "th";
        } else if (name == "h1" || name == "h2" || name == "h3" || name == "p") {
            ensureLineBreak();
            element.tags.push_back(tag(buffer, name == "p" ? "paragraph" : name.c_str()));
        } else if (name == "ul" || name == "ol") {
            ensureLineBreak();
            lists.push_back({name == "ol", 1});
        } else if (name == "li") {
            ensureLineBreak();
            const bool ordered = !lists.empty() && lists.back().ordered;
            const char *listTag = ordered ? "numbered" : "bullet";
            element.tags.push_back(tag(buffer, listTag));
            if (ordered)
                append(std::to_string(lists.back().next++) + ".  ", element.tags);
            else
                append("•  ", element.tags);
        } else if (name == "strong" || name == "b") {
            element.tags.push_back(tag(buffer, "bold"));
        } else if (name == "code") {
            element.tags.push_back(tag(buffer, "code"));
        } else if (name == "span") {
            element.chipClasses = attribute(attributeNames, attributeValues, "class");
            element.chip = hasClass(element.chipClasses, "chip");
        }
        elements.push_back(std::move(element));
    }

    void endElement()
    {
        if (elements.empty())
            return;
        Element element = std::move(elements.back());
        elements.pop_back();
        if (element.chip)
            appendChip(element.chipClasses, element.chipText);
        if (element.name == "tr" && table) {
            table->rows.push_back(std::move(table->current));
        } else if (element.name == "table") {
            renderTable();
            ensureLineBreak();
        } else if (element.name == "li" || element.name == "p" || element.name == "h1"
            || element.name == "h2" || element.name == "h3") {
            ensureLineBreak();
        } else if (element.name == "ul" || element.name == "ol") {
            if (!lists.empty())
                lists.pop_back();
            ensureLineBreak();
        }
    }

    void appendText(std::string text)
    {
        if (inTableCell()) {
            const std::string normalised = collapseWhitespace(text);
            if (!normalised.empty()) {
                std::string &cell = table->current.cells.back();
                if (!cell.empty() && std::isspace(static_cast<unsigned char>(text.front())))
                    cell += ' ';
                cell += normalised;
            }
            return;
        }
        Element *chip = nullptr;
        for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
            if (it->chip) {
                chip = &*it;
                break;
            }
        }
        if (chip) {
            chip->chipText += text;
            return;
        }

        bool inTextBlock = false;
        for (const Element &element : elements) {
            inTextBlock = inTextBlock || element.name == "p" || element.name == "li"
                          || element.name == "h1" || element.name == "h2" || element.name == "h3";
        }
        std::string normalised;
        bool whitespace = false;
        const auto precedingCharacterNeedsSpace = [&] {
            auto end = buffer->end();
            if (end.get_offset() == 0)
                return false;
            end.backward_char();
            return end.get_char() != '\n' && !std::isspace(end.get_char());
        };
        for (const unsigned char character : text) {
            if (std::isspace(character)) {
                whitespace = true;
            } else {
                if (whitespace && (!normalised.empty() || precedingCharacterNeedsSpace()))
                    normalised += ' ';
                normalised += static_cast<char>(character);
                whitespace = false;
            }
        }
        if (whitespace && !normalised.empty())
            normalised += ' ';
        if (!inTextBlock && normalised.empty())
            return;
        append(normalised);
    }
};

bool renderHtml(Gtk::TextView &view, const std::string &html)
{
    HtmlRenderer renderer(view);
    const GMarkupParser parser{HtmlRenderer::start, HtmlRenderer::end, HtmlRenderer::text,
                               nullptr, nullptr};
    GError *error = nullptr;
    GMarkupParseContext *context = g_markup_parse_context_new(&parser, G_MARKUP_TREAT_CDATA_AS_TEXT,
                                                               &renderer, nullptr);
    const bool parsed = g_markup_parse_context_parse(context, html.data(), html.size(), &error)
                        && g_markup_parse_context_end_parse(context, &error);
    g_markup_parse_context_free(context);
    if (error) {
        g_error_free(error);
        return false;
    }
    return parsed;
}

} // namespace

HelpWindow::HelpWindow(Gtk::Window &parent)
    : m_buffer(Gtk::TextBuffer::create())
{
    set_title("LGL DNF Helper Help");
    set_transient_for(parent);
    set_default_size(780, 680);
    set_hide_on_close(true);

    auto *header = Gtk::make_managed<Gtk::HeaderBar>();
    auto *title = Gtk::make_managed<Gtk::Label>("HELP");
    title->add_css_class("wordmark");
    header->set_title_widget(*title);
    set_titlebar(*header);

    auto closeHelp = Gtk::EventControllerKey::create();
    closeHelp->signal_key_pressed().connect(
        [this](const guint keyval, guint, Gdk::ModifierType) {
            if (keyval != GDK_KEY_F1 && keyval != GDK_KEY_Escape)
                return false;
            hide();
            return true;
        },
        false);
    add_controller(closeHelp);

    auto h1 = m_buffer->create_tag("h1");
    h1->property_scale() = 1.6;
    h1->property_weight() = Pango::Weight::SEMIBOLD;
    h1->property_pixels_below_lines() = 10;
    auto h2 = m_buffer->create_tag("h2");
    h2->property_scale() = 1.2;
    h2->property_weight() = Pango::Weight::SEMIBOLD;
    h2->property_pixels_above_lines() = 18;
    h2->property_pixels_below_lines() = 6;
    auto h3 = m_buffer->create_tag("h3");
    h3->property_weight() = Pango::Weight::SEMIBOLD;
    h3->property_pixels_above_lines() = 12;
    h3->property_pixels_below_lines() = 4;
    auto paragraph = m_buffer->create_tag("paragraph");
    paragraph->property_pixels_below_lines() = 8;
    auto bullet = m_buffer->create_tag("bullet");
    bullet->property_left_margin() = 18;
    bullet->property_indent() = -14;
    bullet->property_pixels_below_lines() = 4;
    auto numbered = m_buffer->create_tag("numbered");
    numbered->property_left_margin() = 24;
    numbered->property_indent() = -20;
    numbered->property_pixels_below_lines() = 4;
    auto bold = m_buffer->create_tag("bold");
    bold->property_weight() = Pango::Weight::BOLD;
    m_buffer->create_tag("code");
    setMonoFamily("Adwaita Mono");

    m_view.set_buffer(m_buffer);
    try {
        const auto bytes = Gio::Resource::lookup_data_global(
            "/com/linuxgamerlife/dnfhelper/HELP.html");
        gsize size = 0;
        const auto *data = static_cast<const char *>(bytes->get_data(size));
        if (!renderHtml(m_view, std::string(data, size)))
            m_buffer->set_text("The LGL DNF Helper help file could not be opened.");
    } catch (const Glib::Error &) {
        m_buffer->set_text("The LGL DNF Helper help file could not be opened.");
    }

    m_view.set_editable(false);
    m_view.set_cursor_visible(false);
    m_view.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    m_view.set_left_margin(28);
    m_view.set_right_margin(28);
    m_view.set_top_margin(24);
    m_view.set_bottom_margin(24);
    m_view.add_css_class("help-view");

    auto *scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroller->set_child(m_view);
    set_child(*scroller);
}

void HelpWindow::setMonoFamily(const std::string &family)
{
    for (const char *name : {"code", "table-key"}) {
        if (auto found = m_buffer->get_tag_table()->lookup(name))
            found->property_family() = family;
    }
}

} // namespace dnfhelper
