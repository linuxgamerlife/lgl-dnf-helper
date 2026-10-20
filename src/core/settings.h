#pragma once

#include <glibmm/keyfile.h>

#include <string>

namespace dnfhelper {

// Preferences in a GKeyFile stored in $XDG_CONFIG_HOME/lgl-dnf-helper/settings.ini. Every
// setter saves immediately.
class Settings final {
public:
    Settings();
    explicit Settings(std::string path);

    // Whether the compositor draws the title bar (server-side decorations) and the header
    // bar sits inside the window instead.
    bool systemTitleBar() const;
    void setSystemTitleBar(bool enabled);

    const std::string &path() const { return m_path; }

private:
    void save();

    std::string m_path;
    Glib::RefPtr<Glib::KeyFile> m_file;
};

} // namespace dnfhelper
