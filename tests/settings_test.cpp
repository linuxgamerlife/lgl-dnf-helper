#include "core/settings.h"

#include <glib.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace dnfhelper;

namespace {

struct TemporaryConfig {
    std::string folder;
    std::string path;

    TemporaryConfig()
    {
        gchar *created = g_dir_make_tmp("dnfhelper-settings-XXXXXX", nullptr);
        g_assert_nonnull(created);
        folder = created;
        g_free(created);
        path = folder + "/lgl-dnf-helper/settings.ini";
    }
    ~TemporaryConfig()
    {
        std::error_code error;
        std::filesystem::remove_all(folder, error);
    }
    void write(const std::string &contents) const
    {
        std::filesystem::create_directories(folder + "/lgl-dnf-helper");
        std::ofstream(path) << contents;
    }
};

void missingFileUsesDefaults()
{
    TemporaryConfig config;
    const Settings settings(config.path);
    g_assert_false(settings.systemTitleBar());
    g_assert_false(std::filesystem::exists(config.path));
}

void systemTitleBarPersists()
{
    TemporaryConfig config;
    Settings(config.path).setSystemTitleBar(true);
    g_assert_true(std::filesystem::exists(config.path));
    const auto permissions =
        std::filesystem::status(config.folder + "/lgl-dnf-helper").permissions();
    g_assert_true(permissions == std::filesystem::perms::owner_all);
    g_assert_true(Settings(config.path).systemTitleBar());
    Settings(config.path).setSystemTitleBar(false);
    g_assert_false(Settings(config.path).systemTitleBar());
}

void malformedValuesUseDefaults()
{
    TemporaryConfig config;
    config.write("[window]\nsystemTitleBar=maybe\n");
    g_assert_false(Settings(config.path).systemTitleBar());
    config.write("not a key file\n[[[\n");
    g_assert_false(Settings(config.path).systemTitleBar());
}

} // namespace

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/settings/missing-file-uses-defaults", missingFileUsesDefaults);
    g_test_add_func("/settings/system-title-bar-persists", systemTitleBarPersists);
    g_test_add_func("/settings/malformed-values-use-defaults", malformedValuesUseDefaults);
    return g_test_run();
}
