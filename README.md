<div align="center">

# LGL DNF Helper

**A clear, read-only way to understand the RPM packages on your Fedora system.**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Release](https://img.shields.io/badge/release-2.0.0-purple)](CHANGELOG.md)
[![Fedora](https://img.shields.io/badge/Fedora-44-blue?logo=fedora&logoColor=white)](https://fedoraproject.org)
[![RPM](https://img.shields.io/badge/RPM-available-brightgreen?logo=redhat&logoColor=white)](https://github.com/linuxgamerlife/lgl-dnf-helper/releases)
[![GTK](https://img.shields.io/badge/GTK-4-green?logo=gtk&logoColor=white)](https://www.gtk.org)
[![Support on Ko-fi](https://img.shields.io/badge/Support_on-Ko--fi-FF5E5B?logo=ko-fi&logoColor=white)](https://ko-fi.com/linuxgamerlife)

</div>

---

## Overview

LGL DNF Helper is a graphical Fedora utility for inspecting RPM packages and
DNF5 relationships. Search for a package, command, capability, partial name,
or file path, then see what it requires, what needs it, which files and config
may matter, where it came from, and how it appears in DNF history.

- Inspect package information without changing the system
- Search installed and available packages, providers, commands, and file owners
- Follow your GTK theme, accent colour, and fonts while the app is open
- Open related packages directly from dependency and reverse-dependency rows
- Review package impact without treating it as a removal simulation

---

## How It Works

1. Search for a package name, command, capability, partial name, or file path.
2. Select an installed or available result to open its package details.
3. Explore the details tabs for dependencies, reverse dependencies, files,
   system and user config, history, repository data, and impact.
4. Double-click a dependency or required-by row to inspect the related package
   or its provider.

The application reads local DNF and RPM metadata only. It does not refresh
repositories, install packages, remove packages, update packages, or run as
root.

---

## Features

| Feature | What it does |
|---|---|
| **Package search** | Finds exact names, providers, commands, file owners, and partial package names across installed and available packages |
| **Selected package card** | Shows the package name, version, and status badges for installed, available, third-party, system-sensitive, and install reason |
| **Overview** | Shows version, architecture, repository, size, install time, licence, URL, source RPM, vendor, packager, and description |
| **Dependencies** | Lists runtime dependencies, weak dependencies, build requirements, provides, conflicts, and obsoletes for one package build |
| **Required By** | Shows installed packages that depend on the selected package or capability |
| **Files and config** | Lists installed files, direct-dependency system config, and likely user config, data, and cache paths |
| **History and repository** | Shows transactions, commands, package version changes, installed origin, available repositories, and updates |
| **Impact** | Reports direct dependents, leaf status, autoremove status, and repository availability as a conservative read-only check |
| **Tables** | Sort, resize, copy, and open the context menu with right-click, Menu, or Shift+F10 |
| **Built-in Help** | F1 opens an HTML guide with native tables and coloured package badges; F1 again or Escape closes it |
| **Theme support** | Follows the GTK palette, light or dark mode, accent colour, and monospace font live |
| **Command log** | Records every DNF5 and RPM command the application runs, with its result |

The Impact tab is deliberately conservative. Check with DNF before removing a
package.

---

## Install

The current release is **2.0.0**.

### Recommended: Fedora COPR

Enable the [LGL Toolkit COPR](https://copr.fedorainfracloud.org/coprs/linuxgamerlife/lgl-toolkit/)
and install the application:

```bash
sudo dnf copr enable linuxgamerlife/lgl-toolkit
sudo dnf install lgl-dnf-helper
```

Future releases are installed through normal Fedora system updates.

### Manual downloads

RPM release artifacts are available from
[GitHub Releases](https://github.com/linuxgamerlife/lgl-dnf-helper/releases).

You can also build the application directly from source.

---

## What's New in 2.0.0

- Migrated from Qt 6 to GTK 4 and adopted the LGL GTK application standard
- Follows your desktop GTK theme live, including light or dark mode, accent
  colour, and fonts
- Added a header bar, main menu, optional system title bar, and in-app HTML Help
- Added coloured package badges and native Help tables for shortcuts and detail tabs
- F1 toggles Help and Escape closes it
- Split config into System Config and User Config tabs
- Added full DNF command lines, actions, and version changes to History
- Lists build requirements separately from runtime dependencies
- Opens the providing package when you double-click a dependency capability

See [CHANGELOG.md](CHANGELOG.md) for the complete release history.

---

## Build from Source

DNF Helper is a GTK 4 and libadwaita application built from the repository
root. Its maintained source is in `src/`, resources are in `data/`, and tests
are in `tests/`. It requires gtkmm 4.20, libadwaita 1.6, JSON-GLib, DNF5, and
RPM.

Install the dependencies on Fedora 44:

```bash
sudo dnf install cmake gcc-c++ ninja-build gtkmm4.0-devel libadwaita-devel json-glib-devel dnf5 rpm
```

Clone, build, and test:

```bash
git clone https://github.com/linuxgamerlife/lgl-dnf-helper.git
cd lgl-dnf-helper
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Run it directly from the build directory:

```bash
./build/lgl-dnf-helper
```

Create a source RPM with `make srpm`. It is written to `result/`; COPR uses the
same packaging rule through `.copr/Makefile`.

The former Qt 6 implementation is retained for historical reference under
`legacy/qt/`. It is not built, tested, or included in source RPMs.

---

## Read-only by Design

The application passes DNF5 and RPM arguments directly through
`Gio::Subprocess`, with a 30-second timeout. It only reads local metadata and
never invokes an install, remove, update, repository refresh, or privileged GUI
operation.

The User Config tab is necessarily heuristic because RPM does not track files
in your home directory. It only checks existing paths under XDG config, data,
and cache locations.

---

## License

MIT. See [LICENSE](LICENSE) for details.

---

<div align="center">
Made for <a href="https://fedoraproject.org">Fedora</a> · by <a href="https://www.youtube.com/@linuxgamerlife">LinuxGamerLife</a>
</div>
