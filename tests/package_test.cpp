#include "core/package.h"

#include <glib.h>

#include <string>

using namespace dnfhelper;

namespace {

void formatsNevra()
{
    PackageId id{"dnf5", "0", "5.4.6.0", "1.fc44", "x86_64"};
    g_assert_true(id.evr() == "5.4.6.0-1.fc44");
    g_assert_true(id.nevra() == "dnf5-5.4.6.0-1.fc44.x86_64");
    id.epoch = "2";
    g_assert_true(id.evr() == "2:5.4.6.0-1.fc44");
    const PackageId bare{"bash", "", "", "", ""};
    g_assert_true(bare.nevra() == "bash");
    g_assert_true(bare.isValid());
    const PackageId blank{" ", "", "", "", ""};
    g_assert_false(blank.isValid());
}

void labelsBuildRequires()
{
    g_assert_true(dependencyTypeLabel(DependencyType::Requires) == "Requires");
    g_assert_true(dependencyTypeLabel(DependencyType::BuildRequires) == "Build Requires");
    g_assert_true(dependencyTypeLabel(DependencyType::RequiresPre) == "Requires-pre");
}

void buildsBadges()
{
    Package package;
    package.id.name = "systemd-udev";
    package.installed = true;
    package.installReason = "Dependency";
    package.repoId = "copr:copr.fedorainfracloud.org:someone:thing";
    const auto badges = badgesForPackage(package);
    g_assert_cmpuint(badges.size(), ==, 4);
    g_assert_true(badges[0].text == "Installed" && badges[0].kind == BadgeKind::Positive);
    g_assert_true(badges[1].text == "Dependency" && badges[1].kind == BadgeKind::Plain);
    g_assert_true(badges[2].text == "Third-party");
    g_assert_true(badges[3].text == "System-sensitive"
                  && badges[3].kind == BadgeKind::Negative);

    Package available;
    available.id.name = "foot";
    available.repoId = "updates";
    const auto plain = badgesForPackage(available);
    g_assert_cmpuint(plain.size(), ==, 1);
    g_assert_true(plain[0].text == "Available");
    g_assert_true(isSystemSensitive("NetworkManager-wifi"));
    g_assert_false(isThirdPartyRepo("fedora-cisco-openh264"));
    g_assert_false(isThirdPartyRepo(""));
}

void formatsSizes()
{
    g_assert_true(humanSize(512) == "512 B");
    g_assert_true(humanSize(1536) == "1.5 KiB");
    g_assert_true(humanSize(5LL * 1024 * 1024 * 1024) == "5.0 GiB");
}

void classifiesFiles()
{
    g_assert_true(fileType("/usr/bin/dnf5") == "Executable");
    g_assert_true(fileType("/usr/lib64/libdnf5.so.2") == "Library");
    g_assert_true(fileType("/etc/dnf/dnf.conf") == "Config");
    g_assert_true(fileType("/usr/share/doc/dnf5/README.md") == "Documentation");
    g_assert_true(fileType("/usr/share/licenses/dnf5/COPYING") == "License");
    g_assert_true(fileType("/usr/share/applications/foot.desktop") == "Desktop Entry");
    g_assert_true(fileType("/usr/share/man/man8/dnf5.8.gz") == "Other");
}

} // namespace

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/package/formats-nevra", formatsNevra);
    g_test_add_func("/package/labels-build-requires", labelsBuildRequires);
    g_test_add_func("/package/builds-badges", buildsBadges);
    g_test_add_func("/package/formats-sizes", formatsSizes);
    g_test_add_func("/package/classifies-files", classifiesFiles);
    return g_test_run();
}
