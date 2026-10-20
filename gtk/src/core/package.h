#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Package data as reported by dnf5 and rpm. Port of src/model and src/services of the Qt app.
namespace dnfhelper {

struct PackageId {
    std::string name;
    std::string epoch;
    std::string version;
    std::string release;
    std::string arch;

    // name-[epoch:]version-release.arch, leaving out what is unknown.
    std::string nevra() const;
    // [epoch:]version-release; a zero epoch is left out.
    std::string evr() const;
    bool isValid() const;
};

struct Package {
    PackageId id;
    std::string summary;
    std::string description;
    std::string license;
    std::string url;
    std::string sourceRpm;
    std::string vendor;
    std::string packager;
    std::string repoId;
    std::string installReason;
    bool installed{false};
    std::int64_t installSize{0};
    // Seconds since the Unix epoch; 0 when unknown.
    std::int64_t installTime{0};
};

enum class DependencyType {
    Requires,
    RequiresPre,
    BuildRequires,
    Recommends,
    Suggests,
    Supplements,
    Enhances,
    Provides,
    Conflicts,
    Obsoletes,
    Unknown,
};

std::string dependencyTypeLabel(DependencyType type);

struct DependencyEdge {
    PackageId from;
    std::string capability;
    DependencyType type{DependencyType::Unknown};
    PackageId installedProvider;
    std::vector<PackageId> availableProviders;
    std::string installReason;
    std::string repoId;
};

struct PackageQueryResult {
    enum class MatchKind {
        ExactPackage,
        FileOwner,
        CapabilityProvider,
        AvailablePackage,
        FuzzyPackage,
        Unknown,
    };

    Package package;
    MatchKind matchKind{MatchKind::Unknown};
    std::string matchedText;
};

// One table row of display text.
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;

// Status of a badge, which picks its chip colour.
enum class BadgeKind { Plain, Positive, Neutral, Negative };

struct Badge {
    std::string text;
    BadgeKind kind{BadgeKind::Plain};
};

// Installed or Available, the install reason, Third-party and System-sensitive.
std::vector<Badge> badgesForPackage(const Package &package);
bool isThirdPartyRepo(const std::string &repoId);
bool isSystemSensitive(const std::string &name);

// "12.3 MiB"; bytes are shown without decimals.
std::string humanSize(std::int64_t bytes);

// The Files tab's Type column: Executable, Library, Config, Documentation, License,
// Desktop Entry or Other.
std::string fileType(const std::string &path);

} // namespace dnfhelper
