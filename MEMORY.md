# LGL DNF Helper Working Memory

Use this file to resume context after compaction or token loss.

## Current Version

- Version is `2.0.0` (GTK 4 migration) across `packaging/lgl-dnf-helper.spec`,
  `CMakeLists.txt`, the metainfo `<releases>`, `CHANGELOG.md` and the README.
- The packaged app is the GTK 4 (gtkmm + libadwaita) build in `src/`, following
  `/development/projects/docs/gtk-desktop-application-standard.md`. Reference apps:
  lgl-papercutter, lgl-emoji-picker, lgl-colour-picker.
- The Qt 6 sources are archived in `legacy/qt/`; they are not built or packaged.

Build commands (inside the `fedora-dev1` distrobox, where this repo is at
`/run/host/development/projects/lgl-dnf-helper`):

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build
DNFHELPER_GTK_SNAPSHOT=/tmp/snap ./build/lgl-dnf-helper   # render and quit
make srpm                                                       # SRPM into result/
```

## GTK Layout

- `src/core/` (no widgets, tested): `package` (models, badges, file types),
  `parsers` (dnf5/rpm output), `commandrunner` (Gio::Subprocess, 30 s timeout),
  `backend` (all queries, sigc signals), `settings`, `theme`.
- `src/ui/`: `window` (main window), `datatable` (ColumnView table with sort and
  context menu), `helpwindow` (HELP.html), `themewatcher`.
- Namespace `dnfhelper`, CSS prefix `dh-`/`dh_`, GResource `/com/linuxgamerlife/dnfhelper/`.
- Shortcuts beyond the standard: Ctrl+F (focus search). Ctrl+R was rejected (means reload
  elsewhere, and Reset is destructive).
- Back/Forward navigation (buttons, Alt+Left/Right, mouse buttons) was removed at the
  user's request on 2026-10-19. Clicking a result opens it; double-clicking a dependency
  opens its provider.
- Recorded deviation: the Search button sits in the body beside the search field.

## dnf5 Gotchas (found during the GTK port)

- `repoquery --requires <name>` merges every version, arch and the `.src` package, whose
  Requires are BuildRequires (issue #6). The backend scopes queries: `--installed` when
  installed, else `--available --latest-limit=1 --arch=<native>,noarch` (no arch filter
  for an exact NEVRA). Build Requires come from the source package named by
  `%{sourcerpm}` (`libdnf5` is built from `dnf5`).
- `--queryformat=%{name}` has no newline; always use `%{name}\n`.
- `--leaves <name>` computes leaves within the filtered set, so it always matches. List
  all leaves (and unneeded) and look the package up.
- `rpm --qf %{EPOCH}` prints `(none)`; the parser maps `(none)` to empty.

## Product Direction

- Read-only GTK 4/C++ Fedora package inspection app.
- Do not run GUI as root.
- Do not install/remove/update packages.
- Prefer practical, dense system-utility UI.
- Package details should load only after the user selects a result.

## Recent UI Decisions

- Header button is `Reset`, not `Refresh`.
- Reset clears search, package results, selected package, tables, description, and log.
- Search results area is split into:
  - `Installed Packages`
  - `All Packages`
- `Installed Packages` is a simple package-name list.
- `All Packages` shows `Package`, `Version`, and `Architecture`.
- `All Packages` deduplicates identical NEVRA rows so installed and available copies of the same package/version/arch do not appear twice.
- Search must only populate the left result sections. It must not auto-open package details, even if there is one result.
- History uses `dnf5 history list/info --json` for the full command line.
- Dependency config: `--requires` names (skipping namespaced capabilities such as
  `rpmlib(...)`, `config(...)`, `perl(...)`), filtered to installed with one dnf5 call, then
  one `rpm -q --qf '[%{=NAME}\t%{FILEFLAGS:fflags}\t%{FILENAMES}\n]'` call.
- Package details load only when the user clicks or double-clicks a result.
- Overview has a resizable description area using a vertical `Gtk::Paned`.
- Right-click menu no longer has `Inspect Selected Package/Capability`.
- Right-click rows with local absolute paths can open their containing folder; no elevation is attempted.
- About dialog includes a Ko-fi support link styled like the reference creator backup tool, pointing to `https://ko-fi.com/linuxgamerlife`.
- README has a `Support` section with a Ko-fi image button at the bottom, not near the top.

## Search Behavior

Search runs all of these together and merges them in this order (since 2.0.0; before, it
stopped at the first that matched, so `kineticwe` hid `kineticwe-noctalia`):

1. Exact installed package query.
2. Installed provider/capability query.
3. Available provider/capability query.
4. Fuzzy package-name query using `*term*`, installed and available. `.src` results dropped.

Important: provider lookup must check installed providers first. Example:

- Searching `dnf` should put installed `dnf5` in `Installed Packages`.
- Other available `dnf5` results go into `All Packages`.
- Search results should not populate Overview/Dependencies/etc until the user selects a row.
- `All Packages` intentionally includes installed and non-installed results, but deduplicates identical NEVRA rows.

## Config Behavior

- `System Config` tab: the package's own `/etc` paths (from `rpm -ql`, installed packages
  only) plus `rpm -qc` config of its direct runtime dependencies. `dnf5` shows
  `/etc/dnf/dnf.conf` via `libdnf5` here.
- `User Config` tab: only guessed home-folder paths (XDG config/data/cache) that exist.
  Uses `rpm -ql` names when installed, else the package name alone.
- Until 2.0.0 the Config tab repeated the Related Config rows; the user asked for one tab
  to be user config only.
- `Files` and config RPM calls normalise an available package's NEVRA to its name.

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

- UI: `src/ui/window.cpp`, tables in `src/ui/datatable.cpp`
- Backend: `src/core/backend.cpp`, parsing in `src/core/parsers.cpp`
- Process runner: `src/core/commandrunner.cpp`
- Help text: `HELP.html` (bundled, F1)
- Version metadata: see Current Version above.

## Release Status

- v1.0.0 released 2026-08-16 (last Qt release).
- v2.0.0 (GTK 4 migration, fixes issue #6) prepared 2026-10-19. Not yet committed,
  tagged or published.

## COPR

Project: `linuxgamerlife/lgl-toolkit`

Spec path: `packaging/lgl-dnf-helper.spec`

COPR builds with the `make_srpm` SCM method: `.copr/Makefile` includes the root
`Makefile`, which archives the checked-out tree as `Source0` (`lgl-dnf-helper-<version>.tar.gz`).
No Git tag or remote tarball is needed.

Install instructions:
```bash
sudo dnf copr enable linuxgamerlife/lgl-toolkit
sudo dnf install lgl-dnf-helper
```
