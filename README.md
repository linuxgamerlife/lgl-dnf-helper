# LGL DNF Helper

A Qt6/C++ Fedora utility for inspecting installed RPM packages and DNF5 dependency relationships.

This project is currently an early read-only prototype. It helps users understand:

- What a package is.
- Why it is installed.
- What it requires.
- What requires it.
- Which files it owns.
- Which related dependency config files may matter.
- Which repository it came from.
- Which DNF history transactions mention it.
- Whether it appears to be a leaf, extra package, or autoremove candidate.

## Current Features

- Search by exact package name, RPM capability, provider, absolute file path, or fuzzy package-name match.
- Search results are split into Installed Packages and All Packages; package details load only after you select a result.
- Overview tab with version, arch, repo, install reason, size, source RPM, license, vendor, packager, and package description.
- Dependencies tab with hard dependencies, weak dependencies, provides, conflicts, and obsoletes.
- Required By tab showing installed packages that depend on the selected package/capability.
- Files tab using RPM file ownership data.
- Related Config tab showing RPM config files owned by direct dependency packages.
- Config tab that shows package-owned `/etc` config, likely XDG config/data/cache paths in your home folder, and relevant direct-dependency RPM config.
- History tab using `dnf5 history list --contains-pkgs`.
- Repository tab showing installed origin, available repo matches, extras, and update availability.
- Impact tab with conservative read-only analysis.
- Impact tab includes hover help for terms such as leaf package.
- Back/Forward package navigation, including mouse back/forward buttons.
- Resizable package results/detail panes and resizable Overview description area.
- Right-click table rows to copy cells, rows, or whole tables.
- Right-click path rows to open the containing folder without privilege escalation.
- Double-click dependency and required-by rows to inspect related packages/capabilities.
- Lazy-loaded detail tabs to avoid running every DNF/RPM query on every search.
- Stale async package results are ignored when you navigate away before a query finishes.

The Impact tab is intentionally conservative. It is not a transaction simulation and should not be treated as proof that a package is safe to remove.

The Related Config and Config tabs use RPM metadata from direct dependency packages. For example, `dnf5` does not own `/etc/dnf/dnf.conf` directly, but its `libdnf5` dependency does, so the file is relevant when inspecting `dnf5`.

The user-home part of the Config tab is heuristic. RPM does not track files in your home directory, so these paths are guessed from the package name, shortened package names, owned binaries, desktop files, and AppStream metadata names. The scan is read-only and limited to XDG config/data/cache locations.

## Navigation

- Search only populates the result panes.
- Single-clicking a result previews it without adding it to navigation history.
- Double-clicking a result opens it and adds it to Back/Forward history.
- Double-clicking dependencies or required-by rows opens the related package/capability.
- Back/Forward follows opened package views, not every table click.

## Performance Notes

Package overview loads first. Heavier tabs such as Dependencies, Required By, Related Config, Repository, and Impact load only when opened. This reduces CPU spikes from launching many DNF/RPM queries at once.

## Safety Notes

- The app is read-only.
- It does not run the GUI as root.
- It does not install, remove, update, or modify packages.
- It uses DNF/RPM command arguments directly through `QProcess`, not shell command strings.
- Impact results are a package map, not a removal simulation.

## Build Dependencies

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel
```

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Running

```bash
./build/lgl-dnf-helper
```

The app should not be run with sudo. Read-only package inspection is intended to work as a regular user.

## Support

<p>
  <a href="https://ko-fi.com/linuxgamerlife">
    <img src="https://storage.ko-fi.com/cdn/kofi6.png?v=6" height="36" alt="Support on Ko-fi" />
  </a>
</p>
