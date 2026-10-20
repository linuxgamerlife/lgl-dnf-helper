#include "PackageClassifier.h"

QStringList PackageClassifier::badgesForPackage(const Package &package) const {
    QStringList badges;
    badges << (package.installed ? "Installed" : "Available");

    if (!package.installReason.isEmpty())
        badges << package.installReason;
    if (isThirdPartyRepo(package.repoId))
        badges << "Third-party";
    if (isSystemSensitive(package.id.name))
        badges << "System-sensitive";

    return badges;
}

bool PackageClassifier::isThirdPartyRepo(const QString &repoId) const {
    if (repoId.isEmpty())
        return false;

    const QString lower = repoId.toLower();
    return !(lower == "fedora"
        || lower == "updates"
        || lower == "updates-testing"
        || lower.startsWith("fedora-")
        || lower.startsWith("updates-"));
}

bool PackageClassifier::isSystemSensitive(const QString &name) const {
    static const QStringList sensitivePrefixes = {
        "dnf", "rpm", "glibc", "kernel", "systemd", "grub2",
        "shim", "NetworkManager", "selinux", "sudo", "polkit"
    };

    for (const QString &prefix : sensitivePrefixes) {
        if (name.startsWith(prefix, Qt::CaseInsensitive))
            return true;
    }
    return false;
}
