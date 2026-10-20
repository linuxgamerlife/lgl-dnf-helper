#include "core/package.h"

#include <glib.h>

#include <cstdio>

namespace dnfhelper {

namespace {

bool startsWith(const std::string &text, const std::string &prefix)
{
    return text.rfind(prefix, 0) == 0;
}

bool endsWith(const std::string &text, const std::string &suffix)
{
    return text.size() >= suffix.size()
        && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool contains(const std::string &text, const std::string &part)
{
    return text.find(part) != std::string::npos;
}

std::string lowercase(const std::string &text)
{
    gchar *lower = g_ascii_strdown(text.c_str(), -1);
    std::string result(lower);
    g_free(lower);
    return result;
}

} // namespace

std::string PackageId::evr() const
{
    const std::string versionRelease = release.empty() ? version : version + "-" + release;
    return epoch.empty() || epoch == "0" ? versionRelease : epoch + ":" + versionRelease;
}

std::string PackageId::nevra() const
{
    std::string text = name;
    const std::string evrText = evr();
    if (!evrText.empty())
        text += "-" + evrText;
    if (!arch.empty())
        text += "." + arch;
    return text;
}

bool PackageId::isValid() const
{
    return name.find_first_not_of(" \t\r\n") != std::string::npos;
}

std::string dependencyTypeLabel(const DependencyType type)
{
    switch (type) {
    case DependencyType::Requires: return "Requires";
    case DependencyType::RequiresPre: return "Requires-pre";
    case DependencyType::BuildRequires: return "Build Requires";
    case DependencyType::Recommends: return "Recommends";
    case DependencyType::Suggests: return "Suggests";
    case DependencyType::Supplements: return "Supplements";
    case DependencyType::Enhances: return "Enhances";
    case DependencyType::Provides: return "Provides";
    case DependencyType::Conflicts: return "Conflicts";
    case DependencyType::Obsoletes: return "Obsoletes";
    case DependencyType::Unknown: return "Unknown";
    }
    return "Unknown";
}

std::vector<Badge> badgesForPackage(const Package &package)
{
    std::vector<Badge> badges;
    badges.push_back(package.installed ? Badge{"Installed", BadgeKind::Positive}
                                       : Badge{"Available", BadgeKind::Neutral});
    if (!package.installReason.empty())
        badges.push_back({package.installReason, BadgeKind::Plain});
    if (isThirdPartyRepo(package.repoId))
        badges.push_back({"Third-party", BadgeKind::Neutral});
    if (isSystemSensitive(package.id.name))
        badges.push_back({"System-sensitive", BadgeKind::Negative});
    return badges;
}

bool isThirdPartyRepo(const std::string &repoId)
{
    if (repoId.empty())
        return false;
    const std::string lower = lowercase(repoId);
    return !(lower == "fedora" || lower == "updates" || lower == "updates-testing"
             || startsWith(lower, "fedora-") || startsWith(lower, "updates-"));
}

bool isSystemSensitive(const std::string &name)
{
    static const char *const sensitivePrefixes[] = {
        "dnf", "rpm", "glibc", "kernel", "systemd", "grub2",
        "shim", "networkmanager", "selinux", "sudo", "polkit",
    };
    const std::string lower = lowercase(name);
    for (const char *prefix : sensitivePrefixes) {
        if (startsWith(lower, prefix))
            return true;
    }
    return false;
}

std::string humanSize(const std::int64_t bytes)
{
    const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    char text[48];
    std::snprintf(text, sizeof text, unit == 0 ? "%.0f %s" : "%.1f %s", value, units[unit]);
    return text;
}

std::string fileType(const std::string &path)
{
    if (startsWith(path, "/usr/bin") || startsWith(path, "/usr/sbin"))
        return "Executable";
    if (contains(path, "/lib") && contains(path, ".so"))
        return "Library";
    if (startsWith(path, "/etc"))
        return "Config";
    if (contains(path, "/doc/"))
        return "Documentation";
    if (contains(path, "/licenses/"))
        return "License";
    if (endsWith(path, ".desktop"))
        return "Desktop Entry";
    return "Other";
}

} // namespace dnfhelper
