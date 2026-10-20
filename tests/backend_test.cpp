#include "core/backend.h"

#include <glib.h>

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <string>

using namespace dnfhelper;

namespace {

const char *const nameFormat = "--queryformat=%{name}\\n";

std::string join(const std::string &program, const std::vector<std::string> &arguments)
{
    std::string line = program;
    for (const std::string &argument : arguments)
        line += ' ' + argument;
    return line;
}

bool contains(const std::string &text, const std::string &part)
{
    return text.find(part) != std::string::npos;
}

// Answers commands from a table of canned results (exit 0 with no output by default). Replies
// are queued and delivered by drain(), as the real runner delivers them on the main loop.
class FakeRunner final : public CommandRunner {
public:
    std::map<std::string, CommandResult> responses;
    std::vector<std::string> commands;

    void respond(const std::string &command, const std::string &out, const int exitCode = 0)
    {
        CommandResult result;
        result.exitCode = exitCode;
        result.out = out;
        responses[command] = result;
    }

    void run(const std::string &program, const std::vector<std::string> &arguments,
             CommandCallback done) override
    {
        const std::string command = join(program, arguments);
        commands.push_back(command);
        CommandResult result;
        result.exitCode = 0;
        if (const auto found = responses.find(command); found != responses.end())
            result = found->second;
        m_queue.emplace_back(std::move(done), result);
    }

    void drain()
    {
        while (!m_queue.empty()) {
            auto [done, result] = std::move(m_queue.front());
            m_queue.pop_front();
            done(result);
        }
    }

private:
    std::deque<std::pair<CommandCallback, CommandResult>> m_queue;
};

struct Fixture {
    FakeRunner runner;
    Backend backend{runner, "x86_64", {"/nonexistent/config", "/nonexistent/data",
                                       "/nonexistent/cache"}};
};

std::vector<std::string> requiresQueries(const FakeRunner &runner)
{
    std::vector<std::string> found;
    for (const std::string &command : runner.commands) {
        if (contains(command, " --requires "))
            found.push_back(command);
    }
    return found;
}

// Issue #6: build dependencies are listed as Build Requires, never as Requires.
void separatesBuildRequires()
{
    Fixture f;
    f.runner.respond(std::string("dnf5 repoquery --cacheonly --installed ") + nameFormat
                         + " ananicy-cpp",
                     "ananicy-cpp\n");
    f.runner.respond("dnf5 repoquery --cacheonly --requires --installed ananicy-cpp",
                     "/bin/sh\nananicy-cpp-rules\nlibc.so.6()(64bit)\n");
    f.runner.respond(
        "dnf5 repoquery --cacheonly --queryformat=%{sourcerpm}\\n --installed ananicy-cpp",
        "ananicy-cpp-1.1.1-1.fc44.src.rpm\n");
    f.runner.respond(
        "dnf5 repoquery --cacheonly --arch=src --latest-limit=1 --requires ananicy-cpp",
        "cmake\ngcc-c++\n/bin/sh\n");

    std::vector<DependencyEdge> edges;
    int emitted = 0;
    f.backend.dependenciesLoaded.connect(
        [&](const Request &request, const std::vector<DependencyEdge> &loaded) {
            g_assert_true(request.name == "ananicy-cpp");
            g_assert_cmpuint(request.generation, ==, 7);
            edges = loaded;
            ++emitted;
        });
    f.backend.loadDependencies({"ananicy-cpp", 7});
    f.runner.drain();

    g_assert_cmpint(emitted, ==, 1);
    std::vector<std::string> requires_;
    std::vector<std::string> buildRequires;
    for (const DependencyEdge &edge : edges) {
        if (edge.type == DependencyType::Requires)
            requires_.push_back(edge.capability);
        if (edge.type == DependencyType::BuildRequires)
            buildRequires.push_back(edge.capability);
    }
    g_assert_cmpuint(requires_.size(), ==, 3);
    g_assert_true(std::find(requires_.begin(), requires_.end(), "cmake") == requires_.end());
    g_assert_cmpuint(buildRequires.size(), ==, 3);
    g_assert_true(buildRequires[0] == "cmake");
    // Grouped by type: every Requires row comes before the Build Requires rows.
    g_assert_true(edges.front().type == DependencyType::Requires);
    g_assert_true(edges.back().type == DependencyType::BuildRequires);

    // Every runtime query is limited to the installed build.
    for (const std::string &command : requiresQueries(f.runner)) {
        g_assert_true(contains(command, "--installed") || contains(command, "--arch=src"));
    }
}

// The source package can be named differently from the binary package.
void looksUpBuildRequiresBySourceName()
{
    Fixture f;
    f.runner.respond(std::string("dnf5 repoquery --cacheonly --installed ") + nameFormat
                         + " libdnf5",
                     "libdnf5\n");
    f.runner.respond("dnf5 repoquery --cacheonly --queryformat=%{sourcerpm}\\n --installed libdnf5",
                     "dnf5-5.4.6.0-1.fc44.src.rpm\n");
    f.backend.loadDependencies({"libdnf5", 1});
    f.runner.drain();
    g_assert_true(std::find(f.runner.commands.begin(), f.runner.commands.end(),
                            "dnf5 repoquery --cacheonly --arch=src --latest-limit=1 --requires dnf5")
                  != f.runner.commands.end());
}

// Not installed: the latest build for this machine, not every version and architecture.
void scopesAvailablePackages()
{
    Fixture f;
    f.backend.loadDependencies({"foot", 1});
    f.runner.drain();
    bool sawRequires = false;
    for (const std::string &command : requiresQueries(f.runner)) {
        if (contains(command, "--arch=src"))
            continue;
        sawRequires = true;
        g_assert_true(contains(command, "--available --latest-limit=1 --arch=x86_64,noarch"));
    }
    g_assert_true(sawRequires);

    // An exact NEVRA already names one build, whatever its architecture.
    FakeRunner &runner = f.runner;
    runner.commands.clear();
    f.backend.loadDependencies({"wine-core-10.0-1.fc44.i686", 2});
    runner.drain();
    for (const std::string &command : requiresQueries(runner)) {
        if (!contains(command, "--arch=src"))
            g_assert_false(contains(command, "--arch="));
    }
}

// System Config lists the package's own /etc paths and its runtime dependencies' config, so
// build tools' config is not listed.
void systemConfigSkipsBuildDependencies()
{
    Fixture f;
    f.runner.respond(std::string("dnf5 repoquery --cacheonly --installed ") + nameFormat
                         + " dnf5",
                     "dnf5\n");
    f.runner.respond("dnf5 repoquery --cacheonly --requires --installed dnf5",
                     "libdnf5(x86-64) = 5.4.6.0-1.fc44\nlibc.so.6()(64bit)\ndnf5\n"
                     "rpmlib(PayloadIsZstd) <= 5.4.18-1\nconfig(dnf5) = 5.4.6.0\nmissing\n");
    f.runner.respond(std::string("dnf5 repoquery --cacheonly --installed ") + nameFormat
                         + " libdnf5 missing",
                     "libdnf5\n");
    f.runner.respond(std::string("rpm -q --qf ") + parsers::configQueryFormat + " libdnf5",
                     "libdnf5\tcn\t/etc/dnf/dnf.conf\nlibdnf5\t\t/usr/lib64/libdnf5.so.2\n");
    f.runner.respond("rpm -ql dnf5", "/etc/dnf/dnf5-aliases.d\n/usr/bin/dnf5\n");

    Rows rows;
    f.backend.systemConfigLoaded.connect(
        [&](const Request &, const Rows &loaded) { rows = loaded; });
    f.backend.loadSystemConfig({"dnf5", 1});
    f.runner.drain();

    g_assert_cmpuint(rows.size(), ==, 2);
    g_assert_true(rows[0][0] == "dnf5" && rows[0][1] == "/etc/dnf/dnf5-aliases.d");
    g_assert_true(rows[1][0] == "libdnf5");
    g_assert_true(rows[1][1] == "/etc/dnf/dnf.conf");
    g_assert_true(rows[1][3] == "Owned by direct dependency libdnf5");
    // Namespaced capabilities such as rpmlib(...) and config(...) are never looked up.
    for (const std::string &command : f.runner.commands) {
        g_assert_false(contains(command, "rpmlib") || contains(command, " config"));
        g_assert_false(contains(command, "cmake"));
    }
}

void reportsNoSystemConfig()
{
    Fixture f;
    Rows rows;
    f.backend.systemConfigLoaded.connect(
        [&](const Request &, const Rows &loaded) { rows = loaded; });
    f.backend.loadSystemConfig({"foot", 1});
    f.runner.drain();
    g_assert_cmpuint(rows.size(), ==, 1);
    g_assert_true(rows[0][0] == "No system config found");
    // Not installed: rpm has no file list to read.
    for (const std::string &command : f.runner.commands)
        g_assert_false(command == "rpm -ql foot");
}

// User Config guesses from the package name when the package is not installed.
void userConfigWithoutFileList()
{
    Fixture f;
    f.runner.respond("rpm -ql foot", "", 1);
    Rows rows;
    f.backend.userConfigLoaded.connect([&](const Request &, const Rows &loaded) { rows = loaded; });
    f.backend.loadUserConfig({"foot-1.27.0-1.fc44.x86_64", 1});
    f.runner.drain();
    g_assert_cmpuint(rows.size(), ==, 2);
    g_assert_true(rows[0][0] == "Note");
    g_assert_true(rows[1][0] == "No match");
}

// Exact installed name, then installed and available providers, then a fuzzy name match.
void searchFallsBackInOrder()
{
    const std::string format = "--queryformat=" + parsers::packageQueryFormat();
    Fixture f;
    f.runner.respond("dnf5 repoquery --cacheonly --installed " + format + " --whatprovides dnf",
                     "dnf5|0|5.4.6.0|1.fc44|x86_64|@System|updates|User|1|1|DNF|GPL|||V|P\n");
    f.runner.respond("dnf5 repoquery --cacheonly --available " + format + " --whatprovides dnf",
                     "dnf5|0|5.4.6.0|1.fc44|x86_64|updates|||||DNF|GPL|||V|P\n"
                     "dnf5|0|5.4.6.0|1.fc44|i686|updates|||||DNF|GPL|||V|P\n");
    std::vector<PackageQueryResult> results;
    f.backend.searchCompleted.connect(
        [&](const Request &, const std::vector<PackageQueryResult> &found) { results = found; });
    f.backend.search({"dnf", 1});
    f.runner.drain();
    g_assert_cmpuint(results.size(), ==, 3);
    g_assert_true(results[0].package.installed);
    g_assert_true(results[0].matchKind == PackageQueryResult::MatchKind::CapabilityProvider);
    g_assert_false(results[2].package.installed);

    Fixture fuzzy;
    fuzzy.runner.respond("dnf5 repoquery --cacheonly --available " + format + " *noctalia*",
                         "noctalia-shell|0|1.0|1.fc44|x86_64|copr:x|||||Shell|MIT|||V|P\n");
    results.clear();
    fuzzy.backend.searchCompleted.connect(
        [&](const Request &, const std::vector<PackageQueryResult> &found) { results = found; });
    fuzzy.backend.search({"noctalia", 1});
    fuzzy.runner.drain();
    g_assert_cmpuint(results.size(), ==, 1);
    g_assert_true(results[0].package.id.name == "noctalia-shell");
    g_assert_true(results[0].matchKind == PackageQueryResult::MatchKind::FuzzyPackage);

    // An exact installed match does not hide packages whose names contain the text, and
    // source packages are left out.
    Fixture partial;
    partial.runner.respond("dnf5 repoquery --cacheonly --installed " + format + " kineticwe",
                           "kineticwe|0|0.0.1|19.fc44|x86_64|@System|copr:x|User|1|1|KWE|MIT|||V|P\n");
    partial.runner.respond("dnf5 repoquery --cacheonly --available " + format + " *kineticwe*",
                           "kineticwe|0|0.0.1|1.fc44|src|copr:x|||||KWE|MIT|||V|P\n"
                           "kineticwe-noctalia|0|0.0.1|15.fc44|x86_64|copr:x|||||N|MIT|||V|P\n");
    results.clear();
    partial.backend.searchCompleted.connect(
        [&](const Request &, const std::vector<PackageQueryResult> &found) { results = found; });
    partial.backend.search({"kineticwe", 1});
    partial.runner.drain();
    g_assert_cmpuint(results.size(), ==, 2);
    g_assert_true(results[0].package.id.name == "kineticwe");
    g_assert_true(results[0].matchKind == PackageQueryResult::MatchKind::ExactPackage);
    g_assert_true(results[1].package.id.name == "kineticwe-noctalia");
}

// The full command line comes from dnf5's JSON output.
void historyUsesJson()
{
    Fixture f;
    f.runner.respond("dnf5 history list --json --contains-pkgs=kineticwe",
                     R"([{"id":494},{"id":493}])");
    f.runner.respond(
        "dnf5 history info --json --contains-pkgs=kineticwe 494 493",
        R"([{"id":494,"description":"dnf install ./lgl-kineticwe-copr/results/x86_64/a.rpm",)"
        R"("packages":[{"nevra":"kineticwe-0:0.0.1-18.fc44.x86_64","action":"Install"}]}])");
    Rows rows;
    f.backend.historyLoaded.connect([&](const Request &, const Rows &loaded) { rows = loaded; });
    f.backend.loadHistory({"kineticwe-0.0.1-19.fc44.x86_64", 1});
    f.runner.drain();
    g_assert_cmpuint(rows.size(), ==, 1);
    g_assert_true(rows[0][4] == "dnf install ./lgl-kineticwe-copr/results/x86_64/a.rpm");

    // Older dnf5 without --json: the plain table.
    Fixture plain;
    plain.runner.respond("dnf5 history list --json --contains-pkgs=foot", "Unknown argument", 2);
    plain.runner.respond("dnf5 history list --contains-pkgs=foot",
                         "ID Command line Date and time Action(s) Altered\n"
                         "12 dnf install foot 2026-05-08 12:34:56  3\n");
    plain.backend.historyLoaded.connect([&](const Request &, const Rows &loaded) { rows = loaded; });
    plain.backend.loadHistory({"foot", 1});
    plain.runner.drain();
    g_assert_cmpuint(rows.size(), ==, 1);
    g_assert_true(rows[0][0] == "12");
}

// Dependency capabilities lead to the package that provides them.
void resolvesProviders()
{
    Fixture f;
    f.runner.respond("rpm -q --whatprovides --qf %{NAME}\\n fcft(x86-64)", "fcft\n");
    f.runner.respond("rpm -q --whatprovides --qf %{NAME}\\n missing", "no package provides missing\n", 1);
    std::vector<std::string> found;
    f.backend.resolveProvider("fcft(x86-64) >= 3.3.1",
                              [&](const std::string &provider) { found.push_back(provider); });
    f.backend.resolveProvider("missing",
                              [&](const std::string &provider) { found.push_back(provider); });
    f.runner.drain();
    g_assert_cmpuint(found.size(), ==, 2);
    g_assert_true(found[0] == "fcft");
    g_assert_true(found[1].empty());
}

// rpm takes a package name; an available package is shown by its NEVRA.
void filesUsePackageName()
{
    Fixture f;
    f.backend.loadFiles({"dnf5-5.2.17.0-2.fc43.x86_64", 1});
    f.runner.drain();
    g_assert_true(f.runner.commands.back() == "rpm -ql dnf5");
}

// One name per line: several dependents are counted, not run together.
void impactCountsDependents()
{
    Fixture f;
    f.runner.respond(std::string("dnf5 repoquery --cacheonly --installed ") + nameFormat
                         + " --whatdepends glib2",
                     "gtk4\nlibsoup3\npango\n");
    // Leaves come from the full list: a query for glib2 alone would always list it.
    f.runner.respond(std::string("dnf5 repoquery --cacheonly --leaves ") + nameFormat,
                     "ark\nfoot\n");
    f.runner.respond(std::string("dnf5 repoquery --cacheonly --leaves ") + nameFormat + " glib2",
                     "glib2\n");
    Rows rows;
    f.backend.impactLoaded.connect([&](const Request &, const Rows &loaded) { rows = loaded; });
    f.backend.loadImpact({"glib2", 1});
    f.runner.drain();
    g_assert_cmpuint(rows.size(), ==, 6);
    g_assert_true(contains(rows[1][1], "Yes. 3 installed package(s)"));
    g_assert_true(rows[2][0] == "Leaf package?");
    g_assert_true(contains(rows[2][1], "No."));

    Fixture leaf;
    leaf.runner.respond(std::string("dnf5 repoquery --cacheonly --leaves ") + nameFormat,
                        "ark\nfoot\n");
    leaf.backend.impactLoaded.connect([&](const Request &, const Rows &loaded) { rows = loaded; });
    leaf.backend.loadImpact({"foot", 1});
    leaf.runner.drain();
    g_assert_true(contains(rows[2][1], "Yes."));
}

void reportsFailures()
{
    Fixture f;
    CommandResult timedOut;
    timedOut.failure = "Command timed out";
    f.runner.responses["rpm -ql foot"] = timedOut;
    std::vector<std::string> errors;
    bool finishedOk = true;
    int filesEmitted = 0;
    f.backend.errorOccurred.connect([&](const std::string &message) { errors.push_back(message); });
    f.backend.commandFinished.connect([&](const std::string &, bool ok) { finishedOk = ok; });
    f.backend.filesLoaded.connect(
        [&](const Request &, const std::vector<std::string> &) { ++filesEmitted; });
    f.backend.loadFiles({"foot", 1});
    f.runner.drain();
    g_assert_false(finishedOk);
    g_assert_cmpuint(errors.size(), ==, 1);
    g_assert_true(errors[0] == "Command timed out");
    // The tab is left to load again rather than shown as empty.
    g_assert_cmpint(filesEmitted, ==, 0);
}

// Replies that arrive after the backend is gone are ignored.
void ignoresRepliesAfterDestruction()
{
    FakeRunner runner;
    int emitted = 0;
    {
        Backend backend(runner, "x86_64", {});
        backend.filesLoaded.connect(
            [&](const Request &, const std::vector<std::string> &) { ++emitted; });
        backend.loadFiles({"foot", 1});
    }
    runner.drain();
    g_assert_cmpint(emitted, ==, 0);
}

} // namespace

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/backend/separates-build-requires", separatesBuildRequires);
    g_test_add_func("/backend/looks-up-build-requires-by-source-name",
                    looksUpBuildRequiresBySourceName);
    g_test_add_func("/backend/scopes-available-packages", scopesAvailablePackages);
    g_test_add_func("/backend/system-config-skips-build-dependencies",
                    systemConfigSkipsBuildDependencies);
    g_test_add_func("/backend/reports-no-system-config", reportsNoSystemConfig);
    g_test_add_func("/backend/user-config-without-file-list", userConfigWithoutFileList);
    g_test_add_func("/backend/history-uses-json", historyUsesJson);
    g_test_add_func("/backend/resolves-providers", resolvesProviders);
    g_test_add_func("/backend/search-falls-back-in-order", searchFallsBackInOrder);
    g_test_add_func("/backend/files-use-package-name", filesUsePackageName);
    g_test_add_func("/backend/impact-counts-dependents", impactCountsDependents);
    g_test_add_func("/backend/reports-failures", reportsFailures);
    g_test_add_func("/backend/ignores-replies-after-destruction", ignoresRepliesAfterDestruction);
    return g_test_run();
}
