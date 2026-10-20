#include "core/backend.h"

#include <glibmm/miscutils.h>

#include <sys/utsname.h>

#include <algorithm>
#include <set>

namespace dnfhelper {

using parsers::nonEmptyLines;
using parsers::packageQueryFormat;
using parsers::packagesFromDelimitedOutput;
using parsers::trimmed;

namespace {

using MatchKind = PackageQueryResult::MatchKind;

// One package name per line; without the newline dnf5 runs the names together.
const char *const nameFormat = "--queryformat=%{name}\\n";

std::vector<std::string> concat(std::vector<std::string> first,
                                const std::vector<std::string> &second)
{
    first.insert(first.end(), second.begin(), second.end());
    return first;
}

} // namespace

std::string nativeArchitecture()
{
    utsname name{};
    if (uname(&name) != 0)
        return {};
    const std::string machine = name.machine;
    // RPM names 32-bit x86 builds i686 whatever the kernel reports.
    if (machine == "i386" || machine == "i486" || machine == "i586")
        return "i686";
    return machine;
}

parsers::UserFolders userFolders()
{
    return {Glib::get_user_config_dir(), Glib::get_user_data_dir(), Glib::get_user_cache_dir()};
}

Backend::Backend(CommandRunner &runner)
    : Backend(runner, nativeArchitecture(), userFolders())
{
}

Backend::Backend(CommandRunner &runner, std::string nativeArch, parsers::UserFolders folders)
    : m_runner(runner)
    , m_nativeArch(std::move(nativeArch))
    , m_folders(std::move(folders))
    , m_alive(std::make_shared<bool>(true))
{
}

Backend::~Backend() = default;

void Backend::run(const std::string &program, std::vector<std::string> arguments,
                  Handler handler)
{
    if (program == "dnf5" && !arguments.empty() && arguments.front() == "repoquery")
        arguments.insert(arguments.begin() + 1, "--cacheonly");
    const std::string commandLine = commandLineFor(program, arguments);
    commandStarted.emit(commandLine);
    std::weak_ptr<bool> alive = m_alive;
    m_runner.run(program, arguments,
                 [this, alive, commandLine, handler = std::move(handler)](
                     const CommandResult &result) {
                     if (alive.expired())
                         return;
                     if (!result.failure.empty()) {
                         commandFinished.emit(commandLine + ": " + result.failure, false);
                         errorOccurred.emit(result.failure);
                         return;
                     }
                     commandFinished.emit(commandLine, result.exitCode == 0);
                     handler(result.exitCode, result.out, result.err);
                 });
}

void Backend::reportError(const std::string &err)
{
    const std::string message = trimmed(err);
    if (!message.empty())
        errorOccurred.emit(message);
}

void Backend::checkTools()
{
    toolsChecked.emit(programAvailable("dnf5"), programAvailable("rpm"));
}

// --- search ------------------------------------------------------------------------

void Backend::search(const Request &request)
{
    const std::string query = trimmed(request.name);
    if (query.empty()) {
        searchCompleted.emit(request, {});
        return;
    }

    if (query.front() == '/') {
        run("rpm",
            {"-qf", "--qf",
             "%{NAME}|%{EPOCH}|%{VERSION}|%{RELEASE}|%{ARCH}|@System|||||%{SUMMARY}|%{LICENSE}|"
             "%{URL}|%{SOURCERPM}|%{VENDOR}|%{PACKAGER}\\n",
             query},
            [this, request, query](int exitCode, const std::string &out, const std::string &err) {
                std::vector<PackageQueryResult> results;
                if (exitCode == 0) {
                    for (Package package : packagesFromDelimitedOutput(out)) {
                        package.installed = true;
                        results.push_back({package, MatchKind::FileOwner, query});
                    }
                } else {
                    reportError(err);
                }
                searchCompleted.emit(request, results);
            });
        return;
    }

    // Every way of matching, merged: exact installed name, installed and available providers
    // of the text as a capability, then package names containing it. Run together; the order
    // of the list follows this order.
    const std::string format = "--queryformat=" + packageQueryFormat();
    const std::string fuzzyQuery = "*" + query + "*";
    struct Query {
        std::vector<std::string> arguments;
        bool installed;
        bool provider;
        bool fuzzy;
    };
    const std::vector<Query> queries = {
        {{"repoquery", "--installed", format, query}, true, false, false},
        {{"repoquery", "--installed", format, "--whatprovides", query}, true, true, false},
        {{"repoquery", "--available", format, "--whatprovides", query}, false, true, false},
        {{"repoquery", "--installed", format, fuzzyQuery}, true, false, true},
        {{"repoquery", "--available", format, fuzzyQuery}, false, false, true},
    };
    struct Pending {
        std::size_t remaining{0};
        std::vector<std::vector<PackageQueryResult>> parts;
    };
    auto pending = std::make_shared<Pending>();
    pending->remaining = queries.size();
    pending->parts.resize(queries.size());

    const auto finish = [this, request, pending] {
        if (--pending->remaining != 0)
            return;
        std::vector<PackageQueryResult> results;
        std::set<std::string> seen;
        for (const auto &part : pending->parts) {
            for (const PackageQueryResult &result : part) {
                const std::string key = result.package.id.nevra()
                    + (result.package.installed ? "|installed" : "|available");
                if (seen.insert(key).second)
                    results.push_back(result);
            }
        }
        searchCompleted.emit(request, results);
    };

    for (std::size_t index = 0; index < queries.size(); ++index) {
        const Query &spec = queries[index];
        run("dnf5", spec.arguments,
            [this, pending, finish, index, spec, query, fuzzyQuery](
                int exitCode, const std::string &out, const std::string &err) {
                if (exitCode == 0) {
                    for (Package package : packagesFromDelimitedOutput(out)) {
                        // Source packages cannot be installed or inspected like a build.
                        if (package.id.arch == "src" || package.id.arch == "nosrc")
                            continue;
                        package.installed = spec.installed;
                        MatchKind kind = MatchKind::FuzzyPackage;
                        if (!spec.fuzzy && package.id.name == query)
                            kind = spec.installed ? MatchKind::ExactPackage
                                                  : MatchKind::AvailablePackage;
                        else if (spec.provider)
                            kind = MatchKind::CapabilityProvider;
                        pending->parts[index].push_back(
                            {package, kind, spec.fuzzy ? fuzzyQuery : query});
                    }
                } else {
                    reportError(err);
                }
                finish();
            });
    }
}

// --- package -----------------------------------------------------------------------

void Backend::loadPackage(const Request &request)
{
    const std::string name = request.name;
    const auto emitWithDescription = [this, request, name](Package package,
                                                           const bool installedOnly) {
        run("dnf5",
            {"repoquery", installedOnly ? "--installed" : "--available",
             "--queryformat=%{description}", name},
            [this, request, package](int exitCode, const std::string &out,
                                     const std::string &) mutable {
                if (exitCode == 0)
                    package.description = parsers::descriptionFromOutput(out);
                packageLoaded.emit(request, package);
            });
    };

    run("dnf5", {"repoquery", "--installed", "--queryformat=" + packageQueryFormat(), name},
        [this, request, name, emitWithDescription](int exitCode, const std::string &out,
                                                   const std::string &err) {
            if (exitCode == 0) {
                const auto packages = packagesFromDelimitedOutput(out);
                if (!packages.empty()) {
                    emitWithDescription(packages.front(), true);
                    return;
                }
            } else {
                reportError(err);
            }

            run("dnf5",
                {"repoquery", "--available", "--queryformat=" + packageQueryFormat(), name},
                [this, request, name, emitWithDescription](int availableExit,
                                                           const std::string &availableOut,
                                                           const std::string &availableErr) {
                    if (availableExit == 0) {
                        const auto packages = packagesFromDelimitedOutput(availableOut);
                        if (!packages.empty()) {
                            Package package = packages.front();
                            package.installed = false;
                            emitWithDescription(package, false);
                            return;
                        }
                    } else {
                        reportError(availableErr);
                    }
                    Package package;
                    package.id.name = name;
                    package.summary = "Package metadata not found";
                    packageLoaded.emit(request, package);
                });
        });
}

// --- dependencies ------------------------------------------------------------------

void Backend::withPackageScope(const std::string &name,
                               std::function<void(const std::vector<std::string> &)> next)
{
    run("dnf5", {"repoquery", "--installed", nameFormat, name},
        [this, name, next](int exitCode, const std::string &out, const std::string &) {
            if (exitCode == 0 && !nonEmptyLines(out).empty()) {
                next({"--installed"});
                return;
            }
            std::vector<std::string> scope{"--available", "--latest-limit=1"};
            // An exact NEVRA already names one build.
            if (!parsers::isNevra(name) && !m_nativeArch.empty())
                scope.push_back("--arch=" + m_nativeArch + ",noarch");
            next(scope);
        });
}

void Backend::loadDependencies(const Request &request)
{
    const std::string name = request.name;
    withPackageScope(name, [this, request, name](const std::vector<std::string> &scope) {
        struct Pending {
            int remaining{0};
            std::vector<DependencyEdge> edges;
        };
        auto pending = std::make_shared<Pending>();
        const auto finish = [this, request, pending] {
            if (--pending->remaining != 0)
                return;
            std::stable_sort(pending->edges.begin(), pending->edges.end(),
                             [](const DependencyEdge &a, const DependencyEdge &b) {
                                 return static_cast<int>(a.type) < static_cast<int>(b.type);
                             });
            dependenciesLoaded.emit(request, pending->edges);
        };
        const auto addEdges = [pending](const std::string &out, const DependencyType type,
                                        const std::string &packageName) {
            const auto edges = parsers::dependencyEdgesFromLines(out, type, packageName);
            pending->edges.insert(pending->edges.end(), edges.begin(), edges.end());
        };

        const std::pair<DependencyType, const char *> queries[] = {
            {DependencyType::Requires, "--requires"},
            {DependencyType::RequiresPre, "--requires-pre"},
            {DependencyType::Recommends, "--recommends"},
            {DependencyType::Suggests, "--suggests"},
            {DependencyType::Supplements, "--supplements"},
            {DependencyType::Enhances, "--enhances"},
            {DependencyType::Provides, "--provides"},
            {DependencyType::Conflicts, "--conflicts"},
            {DependencyType::Obsoletes, "--obsoletes"},
        };
        // One more for the source package's build dependencies.
        pending->remaining = static_cast<int>(std::size(queries)) + 1;

        for (const auto &[type, flag] : queries) {
            run("dnf5", concat(concat({"repoquery", flag}, scope), {name}),
                [this, name, type = type, addEdges, finish](int exitCode, const std::string &out,
                                                            const std::string &err) {
                    if (exitCode == 0)
                        addEdges(out, type, name);
                    else
                        reportError(err);
                    finish();
                });
        }

        // BuildRequires are the Requires of the source package the build came from, which is
        // not always named like the binary package (libdnf5 is built from dnf5).
        run("dnf5",
            concat(concat({"repoquery", "--queryformat=%{sourcerpm}\\n"}, scope), {name}),
            [this, name, addEdges, finish](int exitCode, const std::string &out,
                                           const std::string &err) {
                const auto lines = nonEmptyLines(out);
                const std::string source = exitCode == 0 && !lines.empty()
                    ? parsers::sourceNameFromSourceRpm(lines.front()) : std::string();
                if (exitCode != 0)
                    reportError(err);
                if (source.empty()) {
                    finish();
                    return;
                }
                run("dnf5", {"repoquery", "--arch=src", "--latest-limit=1", "--requires", source},
                    [this, name, addEdges, finish](int sourceExit, const std::string &sourceOut,
                                                   const std::string &sourceErr) {
                        if (sourceExit == 0)
                            addEdges(sourceOut, DependencyType::BuildRequires, name);
                        else
                            reportError(sourceErr);
                        finish();
                    });
            });
    });
}

void Backend::loadRequiredBy(const Request &request)
{
    const std::string name = request.name;
    run("dnf5",
        {"repoquery", "--installed", "--queryformat=" + packageQueryFormat(), "--whatdepends",
         name},
        [this, request, name](int exitCode, const std::string &out, const std::string &err) {
            std::vector<DependencyEdge> edges;
            if (exitCode == 0) {
                for (const Package &package : packagesFromDelimitedOutput(out)) {
                    DependencyEdge edge;
                    edge.from.name = name;
                    edge.type = DependencyType::Requires;
                    edge.capability = package.id.name;
                    edge.installedProvider = package.id;
                    edge.installReason = package.installReason;
                    edge.repoId = package.repoId;
                    edges.push_back(std::move(edge));
                }
            } else {
                reportError(err);
            }
            requiredByLoaded.emit(request, edges);
        });
}

// --- files and history ---------------------------------------------------------------

void Backend::loadFiles(const Request &request)
{
    // rpm takes a package name; an available package is shown by its NEVRA.
    run("rpm", {"-ql", parsers::packageNameFromNevra(request.name)},
        [this, request](int exitCode, const std::string &out, const std::string &err) {
            if (exitCode == 0) {
                filesLoaded.emit(request, nonEmptyLines(out));
            } else {
                reportError(err);
                filesLoaded.emit(request, {});
            }
        });
}

void Backend::loadHistory(const Request &request)
{
    // dnf5's table cuts the command line to a column; its JSON has the full command and each
    // package's action and version.
    const std::string name = parsers::packageNameFromNevra(request.name);
    const auto plainList = [this, request, name] {
        run("dnf5", {"history", "list", "--contains-pkgs=" + name},
            [this, request, name](int exitCode, const std::string &out, const std::string &err) {
                if (exitCode == 0) {
                    historyLoaded.emit(request, parsers::historyRowsFromOutput(out, name));
                } else {
                    reportError(err);
                    historyLoaded.emit(request, {});
                }
            });
    };
    run("dnf5", {"history", "list", "--json", "--contains-pkgs=" + name},
        [this, request, name, plainList](int exitCode, const std::string &out,
                                         const std::string &) {
            const auto ids = exitCode == 0 ? parsers::historyIdsFromJson(out) : std::nullopt;
            if (!ids) {
                plainList();
                return;
            }
            if (ids->empty()) {
                historyLoaded.emit(request, parsers::historyRowsFromOutput("", name));
                return;
            }
            run("dnf5", concat({"history", "info", "--json", "--contains-pkgs=" + name}, *ids),
                [this, request, name, plainList](int infoExit, const std::string &infoOut,
                                                 const std::string &) {
                    const auto rows = infoExit == 0
                        ? parsers::historyRowsFromInfoJson(infoOut, name) : std::nullopt;
                    if (rows)
                        historyLoaded.emit(request, *rows);
                    else
                        plainList();
                });
        });
}

// --- repository --------------------------------------------------------------------

void Backend::loadRepositoryInfo(const Request &request)
{
    struct Pending {
        int remaining{4};
        Package installed;
        std::vector<Package> available;
        bool isExtra{false};
        bool hasUpgrade{false};
    };
    auto pending = std::make_shared<Pending>();
    const std::string name = request.name;

    const auto finish = [this, request, pending] {
        if (--pending->remaining != 0)
            return;
        Rows rows;
        if (pending->installed.id.isValid()) {
            std::string installedRepo = pending->installed.repoId;
            const std::string &vendor = pending->installed.vendor;
            // from_repo can name a repo other than the real origin (terra for a package
            // installed from RPM Fusion). An available build with the same vendor in another
            // repo is the more reliable origin.
            if (!vendor.empty()) {
                for (const Package &available : pending->available) {
                    if (available.repoId != installedRepo && available.vendor == vendor) {
                        installedRepo = available.repoId;
                        break;
                    }
                }
            }
            rows.push_back({installedRepo, "Installed from repo", pending->installed.id.evr(),
                            vendor});
            rows.push_back({pending->installed.sourceRpm.empty() ? "Unknown"
                                                                 : pending->installed.sourceRpm,
                            "Source RPM", pending->installed.id.arch,
                            parsers::repoKind(installedRepo)});
        }
        for (const Package &package : pending->available)
            rows.push_back({package.repoId, "Available", package.id.evr(), package.vendor});
        rows.push_back({"Extra package",
                        pending->isExtra ? "Yes: not present in enabled repos" : "No", "", ""});
        rows.push_back({"Update available", pending->hasUpgrade ? "Yes" : "No", "", ""});
        repositoryInfoLoaded.emit(request, rows);
    };

    run("dnf5", {"repoquery", "--installed", "--queryformat=" + packageQueryFormat(), name},
        [this, pending, finish](int exitCode, const std::string &out, const std::string &err) {
            if (exitCode == 0) {
                const auto packages = packagesFromDelimitedOutput(out);
                if (!packages.empty())
                    pending->installed = packages.front();
            } else {
                reportError(err);
            }
            finish();
        });
    run("dnf5", {"repoquery", "--available", "--queryformat=" + packageQueryFormat(), name},
        [this, pending, finish](int exitCode, const std::string &out, const std::string &err) {
            if (exitCode == 0)
                pending->available = packagesFromDelimitedOutput(out);
            else
                reportError(err);
            finish();
        });
    run("dnf5", {"repoquery", "--extras", "--queryformat=" + packageQueryFormat(), name},
        [this, pending, finish](int exitCode, const std::string &out, const std::string &err) {
            if (exitCode == 0)
                pending->isExtra = !packagesFromDelimitedOutput(out).empty();
            else
                reportError(err);
            finish();
        });
    run("dnf5", {"repoquery", "--upgrades", "--queryformat=" + packageQueryFormat(), name},
        [this, pending, finish](int exitCode, const std::string &out, const std::string &err) {
            if (exitCode == 0)
                pending->hasUpgrade = !packagesFromDelimitedOutput(out).empty();
            else
                reportError(err);
            finish();
        });
}

// --- impact ------------------------------------------------------------------------

void Backend::loadImpact(const Request &request)
{
    struct Pending {
        int remaining{4};
        std::size_t requiredByCount{0};
        bool isLeaf{false};
        bool isAutoremove{false};
        bool isExtra{false};
    };
    auto pending = std::make_shared<Pending>();
    const std::string name = request.name;

    const auto finish = [this, request, pending] {
        if (--pending->remaining != 0)
            return;
        const Pending &p = *pending;
        Rows rows;
        rows.push_back({"What this means",
                        "This is a read-only safety check. It helps you understand the package, "
                        "but it is not a remove preview."});
        rows.push_back({"Other packages need it?",
                        p.requiredByCount == 0
                            ? "No installed packages were found depending on it."
                            : "Yes. " + std::to_string(p.requiredByCount)
                                + " installed package(s) appear to depend on it. Check Required "
                                  "By before doing anything."});
        rows.push_back({"Leaf package?",
                        p.isLeaf ? "Yes. DNF says nothing installed needs it directly."
                                 : "No. DNF does not see it as a loose/standalone package."});
        rows.push_back({"Autoremove candidate?",
                        p.isAutoremove ? "Yes. DNF thinks it may no longer be needed."
                                       : "No. DNF does not currently mark it as unneeded."});
        rows.push_back({"Still in enabled repos?",
                        p.isExtra
                            ? "No. It may be from a disabled repo, old upgrade, or local RPM."
                            : "Yes. DNF can still find it in enabled repo metadata."});
        rows.push_back({"Simple advice",
                        p.requiredByCount == 0 && (p.isLeaf || p.isAutoremove)
                            ? "Probably lower risk, but still run a real DNF remove preview "
                              "before removing it."
                            : "Be careful. Look at Required By, Files, and History before "
                              "deciding what this package is for."});
        impactLoaded.emit(request, rows);
    };

    run("dnf5", {"repoquery", "--installed", nameFormat, "--whatdepends", name},
        [this, pending, finish](int exitCode, const std::string &out, const std::string &err) {
            if (exitCode == 0)
                pending->requiredByCount = nonEmptyLines(out).size();
            else
                reportError(err);
            finish();
        });
    // Leaves and unneeded packages are worked out over the packages a query selects, so a
    // query for the package alone always finds it a leaf. List them all and look it up.
    const std::string packageName = parsers::packageNameFromNevra(name);
    const auto listed = [packageName](const std::string &out) {
        const auto lines = nonEmptyLines(out);
        return std::find(lines.begin(), lines.end(), packageName) != lines.end();
    };
    run("dnf5", {"repoquery", "--leaves", nameFormat},
        [this, pending, listed, finish](int exitCode, const std::string &out,
                                        const std::string &err) {
            if (exitCode == 0)
                pending->isLeaf = listed(out);
            else
                reportError(err);
            finish();
        });
    run("dnf5", {"repoquery", "--unneeded", nameFormat},
        [this, pending, listed, finish](int exitCode, const std::string &out,
                                        const std::string &err) {
            if (exitCode == 0)
                pending->isAutoremove = listed(out);
            else
                reportError(err);
            finish();
        });
    run("dnf5", {"repoquery", "--extras", nameFormat, name},
        [this, pending, finish](int exitCode, const std::string &out, const std::string &err) {
            if (exitCode == 0)
                pending->isExtra = !nonEmptyLines(out).empty();
            else
                reportError(err);
            finish();
        });
}

// --- config ------------------------------------------------------------------------

void Backend::collectDependencyConfig(const std::string &name,
                                      const std::vector<std::string> &scope,
                                      std::function<void(const OwnedPaths &)> done)
{
    // Requirements that name a package, then the installed ones among them, then their config
    // files in one rpm call.
    run("dnf5", concat(concat({"repoquery", "--requires"}, scope), {name}),
        [this, name, done](int exitCode, const std::string &out, const std::string &err) {
            std::set<std::string> names;
            if (exitCode == 0) {
                const std::string self = parsers::packageNameFromNevra(name);
                for (const std::string &capability : nonEmptyLines(out)) {
                    const std::string provider =
                        parsers::packageNameFromDependencyCapability(capability);
                    if (!provider.empty() && provider != self)
                        names.insert(provider);
                }
            } else {
                reportError(err);
            }
            if (names.empty()) {
                done({});
                return;
            }
            run("dnf5",
                concat({"repoquery", "--installed", nameFormat},
                       std::vector<std::string>(names.begin(), names.end())),
                [this, done](int installedExit, const std::string &installedOut,
                             const std::string &installedErr) {
                    std::set<std::string> installed;
                    if (installedExit == 0) {
                        for (const std::string &line : nonEmptyLines(installedOut))
                            installed.insert(line);
                    } else {
                        reportError(installedErr);
                    }
                    if (installed.empty()) {
                        done({});
                        return;
                    }
                    run("rpm",
                        concat({"-q", "--qf", parsers::configQueryFormat},
                               std::vector<std::string>(installed.begin(), installed.end())),
                        [done](int, const std::string &configOut, const std::string &) {
                            done(parsers::configFilesFromQueryOutput(configOut));
                        });
                });
        });
}

void Backend::loadUserConfig(const Request &request)
{
    // The package's programs and desktop files suggest more names; without them (a package
    // that is not installed) the guesses come from the package name alone.
    const std::string rpmName = parsers::packageNameFromNevra(request.name);
    run("rpm", {"-ql", rpmName},
        [this, request, rpmName](int exitCode, const std::string &out, const std::string &) {
            const auto files = exitCode == 0 ? nonEmptyLines(out) : std::vector<std::string>();
            userConfigLoaded.emit(request, parsers::userConfigRows(rpmName, files, m_folders));
        });
}

void Backend::loadSystemConfig(const Request &request)
{
    const std::string name = request.name;
    withPackageScope(name, [this, request, name](const std::vector<std::string> &scope) {
        struct Pending {
            int remaining{1};
            std::vector<std::string> files;
            OwnedPaths dependencyConfig;
        };
        auto pending = std::make_shared<Pending>();
        const std::string rpmName = parsers::packageNameFromNevra(name);
        const auto finish = [this, request, pending, rpmName] {
            if (--pending->remaining == 0)
                systemConfigLoaded.emit(request,
                                        parsers::systemConfigRows(rpmName, pending->files,
                                                                  pending->dependencyConfig));
        };
        // Only an installed package has files on disk.
        if (scope.front() == "--installed") {
            ++pending->remaining;
            run("rpm", {"-ql", rpmName},
                [pending, finish](int exitCode, const std::string &out, const std::string &) {
                    if (exitCode == 0)
                        pending->files = nonEmptyLines(out);
                    finish();
                });
        }
        collectDependencyConfig(name, scope, [pending, finish](const OwnedPaths &paths) {
            pending->dependencyConfig = paths;
            finish();
        });
    });
}

void Backend::resolveProvider(const std::string &capability,
                              std::function<void(const std::string &)> done)
{
    const std::string query = parsers::capabilityQuery(capability);
    if (query.empty()) {
        done({});
        return;
    }
    // An installed provider first, else the latest available one.
    run("rpm", {"-q", "--whatprovides", "--qf", "%{NAME}\\n", query},
        [this, query, done](int exitCode, const std::string &out, const std::string &) {
            const auto lines = nonEmptyLines(out);
            if (exitCode == 0 && !lines.empty()) {
                done(lines.front());
                return;
            }
            run("dnf5",
                {"repoquery", "--available", "--latest-limit=1", nameFormat, "--whatprovides",
                 query},
                [done](int availableExit, const std::string &availableOut, const std::string &) {
                    const auto providers = nonEmptyLines(availableOut);
                    done(availableExit == 0 && !providers.empty() ? providers.front()
                                                                  : std::string());
                });
        });
}

} // namespace dnfhelper
