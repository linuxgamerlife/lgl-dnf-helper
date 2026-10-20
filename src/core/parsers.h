#pragma once

#include "core/package.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

// Pure parsing and lookup helpers for dnf5 and rpm output. Port of the static helpers of
// legacy/qt/src/backend/Dnf5CliBackend.cpp, so they can be tested without running dnf5.
namespace dnfhelper::parsers {

// The repoquery --queryformat whose lines packageFromDelimitedLine() reads.
std::string packageQueryFormat();

std::string trimmed(const std::string &text);

// Trimmed lines with the empty ones removed.
std::vector<std::string> nonEmptyLines(const std::string &text);

// One line of packageQueryFormat() output; an invalid id when the line is malformed.
Package packageFromDelimitedLine(const std::string &line);
std::vector<Package> packagesFromDelimitedOutput(const std::string &text);

// One edge per distinct capability line.
std::vector<DependencyEdge> dependencyEdgesFromLines(const std::string &text, DependencyType type,
                                                     const std::string &packageName);

// `dnf5 history list` output as Transaction, Date, Action, Version, Command rows. The
// command is cut to dnf5's column width; used only when JSON output is unavailable.
Rows historyRowsFromOutput(const std::string &text, const std::string &packageName);

// Transaction IDs from `dnf5 history list --json`, newest first; nullopt when the text is not
// that JSON.
std::optional<std::vector<std::string>> historyIdsFromJson(const std::string &text);

// `dnf5 history info --json` as Transaction, Date, Action, Version, Command rows: the full
// command line, and the package's own action and version ("old → new" for an upgrade).
// nullopt when the text is not that JSON.
std::optional<Rows> historyRowsFromInfoJson(const std::string &text,
                                            const std::string &packageName);

// `repoquery --queryformat=%{description}` output without dnf5's progress lines.
std::string descriptionFromOutput(const std::string &text);

// "dnf5-5.2.17.0-2.fc43.x86_64" → "dnf5"; a plain name is returned unchanged.
std::string packageNameFromNevra(const std::string &text);

// True when text is a NEVRA rather than a plain package name.
bool isNevra(const std::string &text);

// "dnf5-5.4.6.0-1.fc44.src.rpm" → "dnf5"; empty for "(none)" or nothing.
std::string sourceNameFromSourceRpm(const std::string &sourceRpm);

// The part of a dependency capability that rpm and dnf5 can look a provider up by:
// "fcft(x86-64) >= 3.3.1" → "fcft(x86-64)", "(foo = 1 if bar)" → "foo".
std::string capabilityQuery(const std::string &capability);

// Fedora, COPR, RPM Fusion, installed system package, third-party or unknown.
std::string repoKind(const std::string &repoId);

// The package a plain capability names ("bash", "python3-gobject >= 3"), or empty for
// files, sonames, ABI markers and other capabilities that are not package names.
std::string packageNameFromDependencyCapability(const std::string &capability);

// The rpm --qf format whose output configFilesFromQueryOutput() reads: one name, flags and
// path line per file.
inline const char *const configQueryFormat = "[%{=NAME}\\t%{FILEFLAGS:fflags}\\t%{FILENAMES}\\n]";

// (package, path) pairs for the files rpm flags as config, sorted.
std::vector<std::pair<std::string, std::string>> configFilesFromQueryOutput(const std::string &text);

// Likely names of an app's folders under the XDG config, data and cache folders.
std::vector<std::string> candidateAppNames(const std::string &packageName,
                                           const std::vector<std::string> &ownedFiles);

struct UserFolders {
    std::string config;
    std::string data;
    std::string cache;
};

// The User Config tab: guessed user config, data and cache paths that exist on disk.
Rows userConfigRows(const std::string &packageName, const std::vector<std::string> &ownedFiles,
                    const UserFolders &folders);

// The System Config tab: the package's own /etc paths, then config files owned by its direct
// dependencies, as Owner, Path, Kind, Why shown rows. dependencyConfig holds (provider, path)
// pairs.
Rows systemConfigRows(const std::string &packageName, const std::vector<std::string> &ownedFiles,
                      const std::vector<std::pair<std::string, std::string>> &dependencyConfig);

} // namespace dnfhelper::parsers
