Name:           lgl-dnf-helper
Version:        2.0.0
Release:        1%{?dist}
Summary:        Inspect RPM packages and DNF5 dependencies

License:        MIT
URL:            https://github.com/linuxgamerlife/lgl-dnf-helper
# Generated from the checked-out commit by `make source` or `make srpm`.
# COPR SCM builds do not require a Git tag.
Source0:        %{name}-%{version}.tar.gz

# The root CMakeLists.txt builds the GTK 4 application in gtk/; the Qt sources
# in src/ are not built.

BuildRequires:  cmake >= 3.20
BuildRequires:  cmake-rpm-macros
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  pkgconfig(gtkmm-4.0) >= 4.20
BuildRequires:  pkgconfig(libadwaita-1) >= 1.6
BuildRequires:  pkgconfig(json-glib-1.0)
BuildRequires:  glib2-devel
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib
Requires:       dnf5
Requires:       rpm

%description
LGL DNF Helper is a graphical utility for inspecting installed RPM packages,
DNF5 dependency relationships, reverse dependencies, file ownership, related
configuration, and repository origin.

It is read-only and does not run as root.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
%cmake_build

%install
%cmake_install

desktop-file-validate \
    %{buildroot}%{_datadir}/applications/com.linuxgamerlife.lgl-dnf-helper.desktop
appstream-util validate-relax --nonet \
    %{buildroot}%{_datadir}/metainfo/com.linuxgamerlife.lgl-dnf-helper.metainfo.xml

%check
%ctest

%files
%license LICENSE
%doc README.md CHANGELOG.md HELP.html
%{_bindir}/lgl-dnf-helper
%{_datadir}/applications/com.linuxgamerlife.lgl-dnf-helper.desktop
%{_datadir}/metainfo/com.linuxgamerlife.lgl-dnf-helper.metainfo.xml
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%{_datadir}/icons/hicolor/64x64/apps/%{name}.png
%{_datadir}/icons/hicolor/128x128/apps/%{name}.png
%{_datadir}/icons/hicolor/256x256/apps/%{name}.png

%changelog
* Mon Oct 19 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 2.0.0-1
- Migrate to GTK 4 (gtkmm) and adopt the LGL GTK app standard
- Follow the desktop theme live: palette, light/dark, accent, fonts
- Add a header bar, a main menu that opens with Alt, a system title bar mode
  (Ctrl+Shift+M), HTML Help with native tables and package badges (F1/Escape),
  and Ctrl+F to focus search
- List build dependencies as Build Requires instead of Requires (#6)
- Show the dependencies of one build instead of every version and architecture
- Always include partial name matches in search
- Split config into System Config and User Config tabs without repeated rows
- Show the full command line, action and version change in History
- Open the providing package when a dependency capability is double-clicked
- Remove Back and Forward navigation
- Stop looking up rpmlib(...) and config(...) capabilities as packages
- Count every dependent package in Impact and correct the leaf package check
- Do not show "(none)" in versions of packages found by file path
- Install the icon at 48, 64, 128 and 256 px and rename the desktop file to
  com.linuxgamerlife.lgl-dnf-helper.desktop

* Sun Aug 16 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 1.0.0-1
- Add sortable package and detail tables
- Add a resizable separator between installed and all-package results
- Focus the package search field on launch and after reset

* Mon Jul 20 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 0.1.3-1
- Fix desktop file using a hardcoded local absolute path for the application icon instead of the installed icon name

* Sun May 24 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 0.1.2-1
- Fix repoquery commands timing out by using cached metadata only
- Fix Required By tab showing duplicate package name instead of install reason and repository
- Fix versioned shared library files (.so.x) classified as Other instead of Library
- Fix About dialog showing hardcoded version instead of application version
- Remove unused this capture in related config lambda
- Set plain text format on package header labels

* Sun May 24 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 0.1.1-1
- Update application icon

* Fri May 08 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 0.1.0-1
- Promote the package inspection workflow to the first minor release

* Fri May 08 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 0.0.2-1
- Add reset action, grouped package results, fuzzy search, package descriptions, config discovery improvements, and improved tab error handling

* Thu May 07 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 0.0.1-1
- Initial read-only prototype
