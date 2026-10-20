#include "core/settings.h"

#include <glib/gstdio.h>
#include <glibmm/miscutils.h>

namespace dnfhelper {

Settings::Settings()
    : Settings(Glib::build_filename(Glib::get_user_config_dir(), "lgl-dnf-helper",
                                    "settings.ini"))
{
}

Settings::Settings(std::string path)
    : m_path(std::move(path))
    , m_file(Glib::KeyFile::create())
{
    try {
        m_file->load_from_file(m_path, Glib::KeyFile::Flags::KEEP_COMMENTS);
    } catch (const Glib::Error &) {
        // Missing or unreadable: start empty.
    }
}

void Settings::save()
{
    const std::string directory = Glib::path_get_dirname(m_path);
    g_mkdir_with_parents(directory.c_str(), 0700);
    try {
        m_file->save_to_file(m_path);
    } catch (const Glib::Error &) {
        // Settings are a convenience; failing to persist them is not fatal.
    }
}

bool Settings::systemTitleBar() const
{
    try {
        return m_file->get_boolean("window", "systemTitleBar");
    } catch (const Glib::Error &) {
        return false;
    }
}

void Settings::setSystemTitleBar(const bool enabled)
{
    m_file->set_boolean("window", "systemTitleBar", enabled);
    save();
}

} // namespace dnfhelper
