#pragma once

#include <gtkmm/textview.h>
#include <gtkmm/window.h>

#include <string>

namespace dnfhelper {

// Shows the embedded HELP.html. The native renderer supports the semantic HTML used by the
// document and maps it onto GTK text tags and chips.
class HelpWindow final : public Gtk::Window {
public:
    explicit HelpWindow(Gtk::Window &parent);

    // Applies the desktop monospace family to headings and code.
    void setMonoFamily(const std::string &family);

private:
    Gtk::TextView m_view;
    Glib::RefPtr<Gtk::TextBuffer> m_buffer;
};

} // namespace dnfhelper
