#include "core/parsers.h"

#include <glib.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

using namespace dnfhelper;

namespace {

bool contains(const std::vector<std::string> &values, const std::string &value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

void parsesDelimitedLines()
{
    const std::string installed =
        "dnf5|0|5.4.6.0|1.fc44|x86_64|@System|updates|User|2457600|1760000000|Command-line "
        "package manager|GPL-2.0-or-later|https://github.com/rpm-software-management/dnf5|"
        "dnf5-5.4.6.0-1.fc44.src.rpm|Fedora Project|Fedora Project";
    const Package package = parsers::packageFromDelimitedLine(installed);
    g_assert_true(package.id.nevra() == "dnf5-5.4.6.0-1.fc44.x86_64");
    g_assert_true(package.repoId == "updates");
    g_assert_true(package.installed);
    g_assert_cmpint(package.installSize, ==, 2457600);
    g_assert_cmpint(package.installTime, ==, 1760000000);
    g_assert_true(package.url == "https://github.com/rpm-software-management/dnf5");

    const Package available = parsers::packageFromDelimitedLine(
        "foot|0|1.27.0|1.fc44|x86_64|updates|||||Fast terminal|MIT|||Fedora Project|Fedora");
    g_assert_false(available.installed);
    g_assert_true(available.repoId == "updates");
    g_assert_cmpint(available.installSize, ==, 0);

    const Package owner = parsers::packageFromDelimitedLine(
        "libdnf5|(none)|5.4.6.0|1.fc44|x86_64|@System|||||Library|LGPL|(none)|src|V|P");
    g_assert_true(owner.id.nevra() == "libdnf5-5.4.6.0-1.fc44.x86_64");
    g_assert_true(owner.url.empty());
    g_assert_true(owner.installed);

    g_assert_false(parsers::packageFromDelimitedLine("too|few|fields").id.isValid());
    const auto packages = parsers::packagesFromDelimitedOutput(
        "Updating and loading repositories:\n" + installed + "\n\nRepositories loaded.\n");
    g_assert_cmpuint(packages.size(), ==, 1);
}

void splitsLines()
{
    const auto lines = parsers::nonEmptyLines("  a \n\n\tb\r\n   \n");
    g_assert_cmpuint(lines.size(), ==, 2);
    g_assert_true(lines[0] == "a" && lines[1] == "b");
}

void dedupesDependencyLines()
{
    const auto edges = parsers::dependencyEdgesFromLines("bash\nbash\n/bin/sh\n",
                                                         DependencyType::Requires, "foo");
    g_assert_cmpuint(edges.size(), ==, 2);
    g_assert_true(edges[0].capability == "bash");
    g_assert_true(edges[1].from.name == "foo");
}

void parsesHistory()
{
    const auto rows = parsers::historyRowsFromOutput(
        "ID Command line                  Date and time       Action(s) Altered\n"
        "12 dnf install foot              2026-05-08 12:34:56           3\n"
        "something unexpected\n",
        "foot");
    g_assert_cmpuint(rows.size(), ==, 2);
    g_assert_true(rows[0][0] == "12");
    g_assert_true(rows[0][1] == "2026-05-08 12:34:56");
    g_assert_true(rows[0][2] == "Changed");
    g_assert_true(rows[0][4] == "dnf install foot");
    g_assert_true(rows[1][2] == "Transaction");

    const auto empty = parsers::historyRowsFromOutput("No transactions found\n", "foot");
    g_assert_cmpuint(empty.size(), ==, 1);
    g_assert_true(empty[0][2] == "No matching history");
}

void cleansDescriptions()
{
    g_assert_true(parsers::descriptionFromOutput("Updating and loading repositories:\n"
                                                 "Repositories loaded.\nFirst line\n\nSecond\n")
                  == "First line\nSecond");
}

void readsNevras()
{
    g_assert_true(parsers::packageNameFromNevra("dnf5-5.2.17.0-2.fc43.x86_64") == "dnf5");
    g_assert_true(parsers::packageNameFromNevra("python3-gobject-1:3.50.0-1.fc44.noarch")
                  == "python3-gobject");
    g_assert_true(parsers::packageNameFromNevra("noctalia-shell") == "noctalia-shell");
    g_assert_true(parsers::isNevra("foot-1.27.0-1.fc44.x86_64"));
    g_assert_false(parsers::isNevra("foot"));
    g_assert_true(parsers::sourceNameFromSourceRpm("dnf5-5.4.6.0-1.fc44.src.rpm") == "dnf5");
    g_assert_true(parsers::sourceNameFromSourceRpm("kernel-6.17.1-300.fc44.nosrc.rpm")
                  == "kernel");
    g_assert_true(parsers::sourceNameFromSourceRpm("(none)").empty());
    g_assert_true(parsers::sourceNameFromSourceRpm("").empty());
}

void classifiesRepos()
{
    g_assert_true(parsers::repoKind("updates") == "Fedora repository");
    g_assert_true(parsers::repoKind("copr:copr.fedorainfracloud.org:a:b") == "COPR repository");
    g_assert_true(parsers::repoKind("rpmfusion-free") == "RPM Fusion repository");
    g_assert_true(parsers::repoKind("@System") == "Installed system package");
    g_assert_true(parsers::repoKind("") == "Unknown");
    g_assert_true(parsers::repoKind("tailscale-stable") == "Third-party repository");
}

void readsCapabilities()
{
    g_assert_true(parsers::packageNameFromDependencyCapability("libdnf5") == "libdnf5");
    g_assert_true(parsers::packageNameFromDependencyCapability("python3-gobject >= 3.40")
                  == "python3-gobject");
    g_assert_true(parsers::packageNameFromDependencyCapability("libdnf5(x86-64) = 5.4.6.0")
                  == "libdnf5");
    g_assert_true(parsers::packageNameFromDependencyCapability("/bin/sh").empty());
    g_assert_true(parsers::packageNameFromDependencyCapability("libc.so.6()(64bit)").empty());
    g_assert_true(parsers::packageNameFromDependencyCapability("rtld(GNU_HASH)").empty());
    g_assert_true(parsers::packageNameFromDependencyCapability("(a if b)").empty());
    // Namespaced capabilities are not packages.
    g_assert_true(
        parsers::packageNameFromDependencyCapability("rpmlib(PayloadIsZstd) <= 5.4.18-1").empty());
    g_assert_true(parsers::packageNameFromDependencyCapability("config(foot) = 1.27.0").empty());
    g_assert_true(parsers::packageNameFromDependencyCapability("perl(File::Path)").empty());
    g_assert_true(parsers::packageNameFromDependencyCapability("pkgconfig(gtk4)").empty());
    g_assert_true(parsers::packageNameFromDependencyCapability("libfoo(aarch-64)") == "libfoo");
}

void guessesAppNames()
{
    const auto names = parsers::candidateAppNames(
        "noctalia-shell", {"/usr/share/applications/org.example.Noctalia.desktop",
                           "/usr/share/metainfo/noctalia-shell.metainfo.xml"});
    g_assert_true(contains(names, "noctalia-shell"));
    g_assert_true(contains(names, "noctalia"));
    g_assert_true(contains(names, "org.example.Noctalia"));
    g_assert_true(contains(names, "Noctalia"));
}

void listsUserConfig()
{
    gchar *created = g_dir_make_tmp("dnfhelper-config-XXXXXX", nullptr);
    g_assert_nonnull(created);
    const std::string folder = created;
    g_free(created);
    std::filesystem::create_directories(folder + "/config/foot");
    std::filesystem::create_directories(folder + "/cache");
    std::ofstream(folder + "/cache/foot") << "x";

    const parsers::UserFolders folders{folder + "/config", folder + "/data", folder + "/cache"};
    const Rows rows = parsers::userConfigRows(
        "foot", {"/etc/xdg/foot/foot.ini", "/etc/xdg/foot/foot.ini", "/usr/bin/foot"}, folders);
    // Only home-folder paths: /etc belongs to the System Config tab.
    g_assert_cmpuint(rows.size(), ==, 3);
    g_assert_true(rows[0][0] == "Note");
    g_assert_true(rows[1][0] == "User Config" && rows[1][2] == "Directory");
    g_assert_true(rows[2][0] == "User Cache" && rows[2][2] == "File");

    const Rows none = parsers::userConfigRows("nothing-here", {}, folders);
    g_assert_cmpuint(none.size(), ==, 2);
    g_assert_true(none[1][0] == "No match");

    std::error_code error;
    std::filesystem::remove_all(folder, error);
}

void readsConfigFiles()
{
    const auto files = parsers::configFilesFromQueryOutput(
        "glibc\tcn\t/etc/ld.so.conf\n"
        "glibc\t\t/usr/lib64/libc.so.6\n"
        "bash\tc\t/etc/skel/.bashrc\n"
        "package rpmlib is not installed\n");
    g_assert_cmpuint(files.size(), ==, 2);
    g_assert_true(files[0].first == "bash" && files[0].second == "/etc/skel/.bashrc");
    g_assert_true(files[1].first == "glibc" && files[1].second == "/etc/ld.so.conf");
}

void listsSystemConfig()
{
    const Rows rows = parsers::systemConfigRows(
        "dnf5", {"/etc/dnf/dnf5-aliases.d", "/usr/bin/dnf5", "/etc/dnf/dnf5-aliases.d"},
        {{"libdnf5", "/etc/dnf/dnf.conf"}, {"libdnf5", "/etc/dnf/dnf5-aliases.d"}});
    g_assert_cmpuint(rows.size(), ==, 2);
    g_assert_true(rows[0][0] == "dnf5" && rows[0][3] == "Owned by this package");
    g_assert_true(rows[1][0] == "libdnf5" && rows[1][1] == "/etc/dnf/dnf.conf");
    g_assert_true(rows[1][3] == "Owned by direct dependency libdnf5");

    const Rows none = parsers::systemConfigRows("foot", {"/usr/bin/foot"}, {});
    g_assert_cmpuint(none.size(), ==, 1);
    g_assert_true(none[0][0] == "No system config found");
}

void readsCapabilityQueries()
{
    g_assert_true(parsers::capabilityQuery("fcft(x86-64) >= 3.3.1") == "fcft(x86-64)");
    g_assert_true(parsers::capabilityQuery("/bin/sh") == "/bin/sh");
    g_assert_true(parsers::capabilityQuery("(foot-terminfo = 1.27.0 if foot-terminfo)")
                  == "foot-terminfo");
    g_assert_true(parsers::capabilityQuery("  ").empty());
}

void parsesHistoryJson()
{
    const auto ids = parsers::historyIdsFromJson(
        R"([{"id":496,"command_line":"dnf upgrade --refresh"},{"id":494}])");
    g_assert_true(ids.has_value());
    g_assert_cmpuint(ids->size(), ==, 2);
    g_assert_true((*ids)[0] == "496" && (*ids)[1] == "494");
    g_assert_true(parsers::historyIdsFromJson("[]")->empty());
    g_assert_false(parsers::historyIdsFromJson("ID Command line").has_value());

    const std::string info = R"([
      {"id":494,"start_time":1792409323,
       "description":"dnf install ./lgl-kineticwe-copr/results/x86_64/kineticwe-0.0.1-18.fc44.x86_64.rpm",
       "packages":[
         {"nevra":"kineticwe-0:0.0.1-18.fc44.x86_64","action":"Upgrade","reason":"User"},
         {"nevra":"kineticwe-noctalia-0:0.0.1-18.fc44.x86_64","action":"Upgrade"},
         {"nevra":"kineticwe-0:0.0.1-17.fc44.x86_64","action":"Replaced","reason":"User"}]},
      {"id":493,"start_time":1792404672,"description":"dnf install kineticwe",
       "packages":[{"nevra":"kineticwe-2:0.0.1-17.fc44.x86_64","action":"Install"}]},
      {"id":12,"description":"dnf remove other","packages":[]}])";
    const auto rows = parsers::historyRowsFromInfoJson(info, "kineticwe");
    g_assert_true(rows.has_value());
    g_assert_cmpuint(rows->size(), ==, 3);
    const Row &upgrade = (*rows)[0];
    g_assert_true(upgrade[0] == "494");
    g_assert_true(upgrade[2] == "Upgrade");
    g_assert_true(upgrade[3] == "0.0.1-17.fc44 → 0.0.1-18.fc44");
    // The full command line, not dnf5's 38-character column.
    g_assert_true(upgrade[4]
                  == "dnf install ./lgl-kineticwe-copr/results/x86_64/kineticwe-0.0.1-18.fc44"
                     ".x86_64.rpm");
    g_assert_true((*rows)[1][2] == "Install" && (*rows)[1][3] == "2:0.0.1-17.fc44");
    g_assert_true((*rows)[2][2] == "Changed" && (*rows)[2][1] == "—");
    g_assert_false(parsers::historyRowsFromInfoJson("not json", "x").has_value());
}

} // namespace

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/parsers/parses-delimited-lines", parsesDelimitedLines);
    g_test_add_func("/parsers/splits-lines", splitsLines);
    g_test_add_func("/parsers/dedupes-dependency-lines", dedupesDependencyLines);
    g_test_add_func("/parsers/parses-history", parsesHistory);
    g_test_add_func("/parsers/cleans-descriptions", cleansDescriptions);
    g_test_add_func("/parsers/reads-nevras", readsNevras);
    g_test_add_func("/parsers/classifies-repos", classifiesRepos);
    g_test_add_func("/parsers/reads-capabilities", readsCapabilities);
    g_test_add_func("/parsers/guesses-app-names", guessesAppNames);
    g_test_add_func("/parsers/lists-user-config", listsUserConfig);
    g_test_add_func("/parsers/lists-system-config", listsSystemConfig);
    g_test_add_func("/parsers/reads-config-files", readsConfigFiles);
    g_test_add_func("/parsers/reads-capability-queries", readsCapabilityQueries);
    g_test_add_func("/parsers/parses-history-json", parsesHistoryJson);
    return g_test_run();
}
