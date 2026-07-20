# LGL DNF Helper Working Memory

Use this file to resume context after compaction or token loss.

## Current Version

- Version is `0.1.1` across all metadata.
- Source is at the project root (no versioned subdirectory).

Build commands:

```bash
cd /development/projects/lgl-dnf-helper
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Product Direction

- Read-only Qt6/C++ Fedora package inspection app.
- Do not run GUI as root.
- Do not install/remove/update packages.
- Prefer practical, dense system-utility UI.
- Package details should load only after the user selects a result.

## Recent UI Decisions

- Header button is `Reset`, not `Refresh`.
- Reset clears search, package results, selected package, tables, description, navigation history, and log.
- Search results area is split into:
  - `Installed Packages`
  - `All Packages`
- `Installed Packages` is a simple package-name list.
- `All Packages` shows `Package`, `Version`, and `Architecture`.
- `All Packages` deduplicates identical NEVRA rows so installed and available copies of the same package/version/arch do not appear twice.
- Search must only populate the left result sections. It must not auto-open package details, even if there is one result.
- Package details load only when the user clicks or double-clicks a result.
- Overview has a resizable description area using a vertical `QSplitter`.
- Right-click menu no longer has `Inspect Selected Package/Capability`.
- Right-click rows with local absolute paths can `Open Containing Folder` via `QDesktopServices`; no elevation is attempted.
- About dialog includes a Ko-fi support link styled like the reference creator backup tool, pointing to `https://ko-fi.com/linuxgamerlife`.
- README has a `Support` section with a Ko-fi image button at the bottom, not near the top.

## Search Behavior

Current search fallback order:

1. Exact installed package query.
2. Installed provider/capability query.
3. Available provider/capability query.
4. Fuzzy package-name query using `*term*`.

This was added so searching `noctalia` can find `noctalia-shell`.

Important: provider lookup must check installed providers first. Example:

- Searching `dnf` should put installed `dnf5` in `Installed Packages`.
- Other available `dnf5` results go into `All Packages`.
- Search results should not populate Overview/Dependencies/etc until the user selects a row.
- `All Packages` intentionally includes installed and non-installed results, but deduplicates identical NEVRA rows.

## Config Behavior

- `Config` tab now shows:
  - package-owned `/etc` config paths,
  - guessed user XDG config/data/cache paths,
  - config owned by direct dependency packages.
- This was restored because `dnf5` users expect `/etc/dnf/dnf.conf` to appear when inspecting `dnf5`, even though the file is actually owned by `libdnf5`.
- Example:
  - `rpm -qf /etc/dnf/dnf.conf` returns `libdnf5`.
  - `rpm -qc libdnf5` returns `/etc/dnf/dnf.conf`.
  - In `dnf5` Config tab it should show as `Related Config`, owned by direct dependency `libdnf5`.
- `Files` and `Config` RPM calls must normalize selected available-package NEVRAs back to package name before calling RPM.
  - Bad: `rpm -ql dnf5-5.2.17.0-2.fc43.x86_64`
  - Good: `rpm -ql dnf5`

## Known Examples

- `dnf`
  - Search term is a capability/command.
  - Installed provider is `dnf5`.
  - Config should include `/etc/dnf/dnf.conf` via `libdnf5`.
- `foot`
  - Not installed in the observed environment.
  - Should have no result in `Installed Packages`.
  - Available versions appear in `All Packages`.
- `noctalia`
  - Exact search may find nothing.
  - Fuzzy fallback should find `noctalia-shell` and related packages.

## Important Files

- UI: `src/ui/MainWindow.cpp`
- UI declarations: `src/ui/MainWindow.h`
- Backend: `src/backend/Dnf5CliBackend.cpp`
- Process wrapper: `src/backend/Dnf5Process.cpp`
- Models: `src/model/`
- Version metadata:
  - `CMakeLists.txt`
  - `src/main.cpp`
  - `src/ui/MainWindow.cpp` About dialog
  - `packaging/lgl-dnf-helper.spec`
  - `packaging/com.linuxgamerlife.lgl-dnf-helper.metainfo.xml`
  - `CHANGELOG.md`

## Release Status

- v0.1.0 released 2026-05-08. Published to COPR and GitHub.
- v0.1.1 in progress (icon update). Not yet tagged or published.

## COPR

Project: `linuxgamerlife/lgl-dnf-helper`

**ACTION REQUIRED**: COPR SCM subdirectory was previously set to `v0.1.0`. That directory no longer exists — source is now at the repo root. Update COPR project SCM settings: clear subdirectory to `/` or leave blank.

Spec path: `packaging/lgl-dnf-helper.spec`

Source0 uses a remote GitHub tar.gz URL — rpkg downloads it, does not use git archive. Source0 was changed from `.zip` to `.tar.gz` in v0.1.1.

Install instructions:
```bash
sudo dnf copr enable linuxgamerlife/lgl-dnf-helper
sudo dnf install lgl-dnf-helper
```
