# Changelog

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
