#pragma once

#include "core/commandrunner.h"
#include "core/package.h"
#include "core/parsers.h"

#include <sigc++/sigc++.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dnfhelper {

// What a reply belongs to. The window tags each request with a generation and drops replies
// for a request it has moved on from.
struct Request {
    std::string name;
    std::uint64_t generation{0};
};

// Read-only package queries through the dnf5 and rpm command-line tools. Port of
// legacy/qt/src/backend/Dnf5CliBackend.cpp. Every query is asynchronous; results arrive
// through the signals on the main loop. repoquery always runs with --cacheonly.
class Backend final {
public:
    explicit Backend(CommandRunner &runner);
    // For tests: the architecture used to scope available packages and the user folders the
    // Config tab looks in.
    Backend(CommandRunner &runner, std::string nativeArch, parsers::UserFolders folders);
    ~Backend();
    Backend(const Backend &) = delete;
    Backend &operator=(const Backend &) = delete;

    void checkTools();
    void search(const Request &request);
    void loadPackage(const Request &request);
    void loadDependencies(const Request &request);
    void loadRequiredBy(const Request &request);
    void loadFiles(const Request &request);
    void loadHistory(const Request &request);
    void loadRepositoryInfo(const Request &request);
    void loadImpact(const Request &request);
    void loadUserConfig(const Request &request);
    void loadSystemConfig(const Request &request);
    // The package that provides a dependency capability (a file, soname or versioned name):
    // the installed provider, else the latest available one; empty when there is none.
    void resolveProvider(const std::string &capability,
                         std::function<void(const std::string &)> done);

    sigc::signal<void(bool dnf5Available, bool rpmAvailable)> toolsChecked;
    sigc::signal<void(const Request &, const std::vector<PackageQueryResult> &)> searchCompleted;
    sigc::signal<void(const Request &, const Package &)> packageLoaded;
    sigc::signal<void(const Request &, const std::vector<DependencyEdge> &)> dependenciesLoaded;
    sigc::signal<void(const Request &, const std::vector<DependencyEdge> &)> requiredByLoaded;
    sigc::signal<void(const Request &, const std::vector<std::string> &)> filesLoaded;
    sigc::signal<void(const Request &, const Rows &)> historyLoaded;
    sigc::signal<void(const Request &, const Rows &)> repositoryInfoLoaded;
    sigc::signal<void(const Request &, const Rows &)> impactLoaded;
    sigc::signal<void(const Request &, const Rows &)> userConfigLoaded;
    sigc::signal<void(const Request &, const Rows &)> systemConfigLoaded;
    sigc::signal<void(const std::string &commandLine)> commandStarted;
    sigc::signal<void(const std::string &summary, bool ok)> commandFinished;
    sigc::signal<void(const std::string &message)> errorOccurred;

private:
    using Handler = std::function<void(int exitCode, const std::string &out,
                                       const std::string &err)>;

    // Runs a command; the handler is not called when the command could not run to completion
    // (failed to start, crashed or timed out), which is reported as an error instead.
    void run(const std::string &program, std::vector<std::string> arguments, Handler handler);
    void reportError(const std::string &err);

    // The repoquery filters that limit a package's own metadata to one build: the installed
    // package, or else the latest available build for this architecture (or the exact NEVRA).
    // Without them repoquery merges every version, every architecture and the source package,
    // whose Requires are its BuildRequires.
    void withPackageScope(const std::string &name,
                          std::function<void(const std::vector<std::string> &scope)> next);
    // Config files owned by the installed direct runtime dependencies of a package, as
    // (provider, path) pairs sorted by provider and path.
    using OwnedPaths = std::vector<std::pair<std::string, std::string>>;
    void collectDependencyConfig(const std::string &name, const std::vector<std::string> &scope,
                                 std::function<void(const OwnedPaths &)> done);

    CommandRunner &m_runner;
    std::string m_nativeArch;
    parsers::UserFolders m_folders;
    // Replies that arrive after the backend is destroyed are ignored.
    std::shared_ptr<bool> m_alive;
};

// The machine architecture as RPM names it (x86_64, aarch64, …).
std::string nativeArchitecture();
// The XDG config, data and cache folders.
parsers::UserFolders userFolders();

} // namespace dnfhelper
