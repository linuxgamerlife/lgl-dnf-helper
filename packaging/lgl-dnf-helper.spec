Name:           lgl-dnf-helper
Version:        0.1.0
Release:        1%{?dist}
Summary:        Qt6 GUI for inspecting RPM packages and DNF5 dependencies

License:        MIT
URL:            https://github.com/linuxgamerlife/lgl-dnf-helper
Source0:        https://github.com/linuxgamerlife/%{name}/archive/refs/tags/v%{version}.zip
BuildRequires:  cmake >= 3.16
BuildRequires:  unzip
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
Requires:       qt6-qtbase
Requires:       dnf5
Requires:       rpm
Recommends:     dnf5daemon-server
Recommends:     polkit

%description
LGL DNF Helper is a Qt6 graphical utility for inspecting installed RPM
packages, DNF5 dependency relationships, reverse dependencies, file
ownership, related configuration, and repository origin.

The initial version is read-only and does not run the GUI as root.

%prep
%autosetup -n %{name}-%{version}

%build
mkdir -p build
cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=%{_prefix}
%make_build

%install
install -Dm755 build/lgl-dnf-helper \
    %{buildroot}%{_bindir}/lgl-dnf-helper

install -Dm644 data/lgl-dnf-helper.desktop \
    %{buildroot}%{_datadir}/applications/lgl-dnf-helper.desktop

install -Dm644 resources/icons/lgl-dnf-helper-icon.png \
    %{buildroot}%{_datadir}/icons/hicolor/512x512/apps/lgl-dnf-helper.png

install -Dm644 packaging/com.linuxgamerlife.lgl-dnf-helper.metainfo.xml \
    %{buildroot}%{_datadir}/metainfo/com.linuxgamerlife.lgl-dnf-helper.metainfo.xml

%post
if [ -x /usr/bin/gtk-update-icon-cache ]; then
    gtk-update-icon-cache -f -t %{_datadir}/icons/hicolor &>/dev/null || :
fi
if [ -x /usr/bin/update-desktop-database ]; then
    update-desktop-database -q %{_datadir}/applications &>/dev/null || :
fi
if [ -x /usr/bin/appstreamcli ]; then
    appstreamcli refresh --force &>/dev/null || :
fi
if [ -x /usr/bin/kbuildsycoca6 ]; then
    kbuildsycoca6 &>/dev/null || :
fi

%postun
if [ -x /usr/bin/gtk-update-icon-cache ]; then
    gtk-update-icon-cache -f -t %{_datadir}/icons/hicolor &>/dev/null || :
fi
if [ -x /usr/bin/update-desktop-database ]; then
    update-desktop-database -q %{_datadir}/applications &>/dev/null || :
fi
if [ -x /usr/bin/appstreamcli ]; then
    appstreamcli refresh --force &>/dev/null || :
fi

%files
%license LICENSE
%doc README.md
%{_bindir}/lgl-dnf-helper
%{_datadir}/applications/lgl-dnf-helper.desktop
%{_datadir}/icons/hicolor/512x512/apps/lgl-dnf-helper.png
%{_datadir}/metainfo/com.linuxgamerlife.lgl-dnf-helper.metainfo.xml

%changelog
* Fri May 08 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 0.1.0-1
- Promote the package inspection workflow to the first minor release

* Fri May 08 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 0.0.2-1
- Add reset action, grouped package results, fuzzy search, package descriptions, config discovery improvements, and improved tab error handling

* Thu May 07 2026 LinuxGamerLife <contact@linuxgamerlife.com> - 0.0.1-1
- Initial read-only prototype
