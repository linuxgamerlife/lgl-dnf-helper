# Changelog

## v2.0.0 - 2026-10-19

### Changed

- Migrated from Qt 6 to GTK 4 (gtkmm) and adopted the LGL GTK app standard. The app follows the desktop theme live: palette, light or dark mode, accent colour, and monospace font.
- Header bar with Reset, a status chip, and the main menu (Clear Log, Use System Title Bar, Help, About, Quit). Tap Alt or press F10 to open or close the menu.
- Optional system title bar (Ctrl+Shift+M), remembered in `~/.config/lgl-dnf-helper/settings.ini`.
- In-app HTML Help with clearer sections, native tables for shortcuts and detail tabs, and coloured package badges that match the selected-package card. Press `F1` again or `Escape` to close it.
- New shortcuts: Ctrl+F to focus search, Menu or Shift+F10 for a table row's menu.
- Removed Back and Forward navigation (the buttons, Alt+Left and Alt+Right, and the mouse back and forward buttons). Clicking a result or double-clicking a dependency opens it directly.
- Status bar messages are now notices at the bottom of the window.
- The Dependencies, Related Config, and Config tabs read one build of the package: the installed build, or else the latest available build for this architecture. They used to merge every version and architecture.
- Search always includes packages whose names contain the text, even when an installed package has exactly that name (searching `kineticwe` also finds `kineticwe-noctalia`). Source packages are left out.
- The config tabs are now **System Config** (the package's own `/etc` paths and its direct dependencies' config) and **User Config** (guessed home-folder paths only). The old Config tab repeated the Related Config rows.
- History shows the full command line, the package's own action, and its version change, from dnf5's JSON output.
- Double-clicking a dependency opens the package that provides it, so `/bin/sh` opens `bash` instead of failing.
- The desktop file is now `com.linuxgamerlife.lgl-dnf-helper.desktop`; re-pin the app if you pinned it. Icons are installed at 48, 64, 128, and 256 px.
- Builds come from the checked-out source (`make srpm`); COPR no longer needs a Git tag.

### Fixed

- Build dependencies were listed as Requires. They now appear as Build Requires and are no longer treated as direct dependencies by Related Config and Config (#6).
- Impact counted at most one dependent package, because dnf5 printed the names on one line.
- Impact reported every installed package as a leaf package.
- Packages found by file path showed "(none)" in their version.
- User cache guesses looked in an app-specific cache folder instead of `~/.cache`.
- Double-clicking a result loaded the package twice.
- Config lookups treated capabilities such as `rpmlib(PayloadIsZstd)` and `config(foot)` as packages named `rpmlib` and `config`, and ran one `rpm` call per dependency; they now use three calls in total.
- History for an available package searched for its full NEVRA and found nothing.

## v1.0.0 - 2026-08-16

### Changed

- Package result panes can now be resized vertically with a draggable separator.
- Search and detail tables can now be sorted by clicking their column headers.
- The package search field now receives keyboard focus when the app opens and after a reset.

## v0.1.3 - 2026-07-20

### Fixed

- Desktop entry (`data/lgl-dnf-helper.desktop`) referenced the application icon by a hardcoded local absolute path instead of the installed icon name (`lgl-dnf-helper`), breaking icon lookup outside the original build machine.

## v0.1.2 - 2026-05-24

### Fixed

- `dnf5 repoquery` commands timing out: all repoquery calls now use `--cacheonly` to read local metadata only and never trigger a network refresh.
- Required By tab showing the package name twice: the "Installed Reason" column now shows the actual install reason (user/dependency) and the "Repository" column now shows the source repo.
- Versioned shared library files (`.so.1`, `.so.6.0.2`, etc.) were classified as "Other" in the Files tab instead of "Library".
- About dialog was showing a hardcoded version string instead of the running application version.

### Changed

- Removed an unnecessary `this` capture in the related config dependency lambda.
- Package header labels (`packageTitle`, `packageNevra`, `packageBadges`) now explicitly use plain text format.

## v0.1.1 - 2026-05-24

### Changed

- Updated application icon.

## v0.1.0 - 2026-05-08

First minor release of the read-only package inspection workflow.

### Notes

- Carries forward the grouped search results, fuzzy matching, package descriptions, config discovery improvements, and read-only DNF/RPM inspection behavior from the 0.0.x prototype series.
- No privileged package-management actions are included.

## v0.0.2 - 2026-05-08

### Added

- Reset button that clears the search, selected package, navigation history, package tables, description, and log.
- Package description box at the bottom of the Overview tab.
- Split search results into Installed Packages and All Packages sections.
- Version and Architecture columns in All Packages.
- Fuzzy package-name fallback so partial searches such as `noctalia` can find packages such as `noctalia-shell`.
- Right-click action to open the containing folder for rows that contain local filesystem paths.

### Fixed

- Available-only package results now load available metadata instead of showing the same installed-package placeholder.
- Provider searches now check installed providers before available providers.
- Search no longer auto-opens details; details load only after the user selects a result.
- All Packages now deduplicates identical package/version/architecture rows.
- Config now includes package-owned `/etc` config and direct dependency config relevant to the selected package.
- RPM-backed tabs normalize selected NEVRA values back to package names before calling RPM.
- Config tab RPM failures now show the underlying error in the log and table.
- Failed Config tab loads are not cached as successful, so revisiting the tab can retry the check.
- Failed commands clear in-flight tab state and leave the header status in an error state.

## v0.0.1 - 2026-05-07

Initial read-only prototype of LGL DNF Helper.

### Added

- Qt6/C++ application scaffold with CMake.
- LGL-style desktop UI with header, search, package results, detail tabs, status bar, and log.
- Read-only DNF5/RPM backend using asynchronous `QProcess`.
- Package search by package name, provider/capability, and absolute file path.
- Overview tab with package metadata from DNF5 queryformat output.
- Dependencies tab covering requires, requires-pre, recommends, suggests, supplements, enhances, provides, conflicts, and obsoletes.
- Required By tab for installed packages depending on the selected package/capability.
- Files tab using RPM-owned file data.
- Related Config tab for RPM config files owned by direct dependency packages.
- Config tab for guessed user XDG config/data/cache paths.
- History tab using DNF5 history package matching.
- Repository tab for installed origin, available repo matches, extras, and update availability.
- Impact tab with conservative read-only checks for required-by count, leaf status, unneeded/autoremove status, extra status, and simple advice.
- Hover explanation for leaf package meaning.
- Back/Forward package navigation and mouse back/forward button support.
- Resizable results/detail panes.
- Right-click copy actions for table cells, rows, and whole tables.
- URL opening from overview table rows.
- Lazy-loaded package detail tabs to reduce CPU spikes.
- Request-name checks on backend results so stale async responses do not update the wrong package view.
- Fedora desktop entry, AppStream metadata, RPM spec, SVG icon, and MIT license.

### Fixed

- Prevented bogus dependency config checks such as `rpm -qc rtld`.
- Prevented process timeout/error paths from emitting duplicate finished/failure results.
- Avoided invalid DNF5 option combinations for leaves and extras queries.
- Replaced noisy autoremove probing with `repoquery --unneeded`.
- Increased compact table row height to avoid clipping text.

### Notes

- The app is read-only and should not be run with sudo.
- Impact is not a DNF transaction simulation.
- Config tab user paths are heuristic because RPM does not track files in home directories.
