# Qt6 C++ DNF Helper App Plan

## Scope

Build a Fedora desktop app in Qt6/C++ that helps users understand installed RPM packages through DNF5 data. The first version should be read-only: package lookup, dependency explanation, reverse dependency explanation, file ownership, package history, repository origin, and removal impact preview if it can be safely simulated.

Do not make the initial app a full package manager. Mutating actions can come later behind DNF5 daemon and Polkit integration.

## Product Shape

Working title: **LGL DNF Helper**

Primary user question:

> What is this RPM, why is it installed, what depends on it, and is it safe to remove?

Main workflow:

1. User searches for a package, command, library, capability, or file path.
2. App resolves the input to one or more RPM packages.
3. User selects a package.
4. App shows a clear package report across several tabs.

## Recommended Stack

- Language: C++20 or C++23.
- UI: Qt 6 Widgets for a practical Fedora desktop utility.
- Build system: CMake.
- Package data backend, phase 1: DNF5 CLI JSON/subprocess where available, with structured wrappers.
- Package data backend, phase 2: libdnf5 C++ API if the API is stable enough for the required queries.
- Privileged operations, future phase: `dnf5daemon-server` over D-Bus with Polkit.

Qt Widgets is a better first fit than QML because this app is table-heavy, tree-heavy, and utility-focused.

## Sudo Model

The main GUI should not run with sudo.

Read-only features should run as the regular user:

- Query installed packages.
- Query package metadata.
- Query dependencies.
- Query reverse dependencies.
- List files owned by a package.
- Read DNF history when permitted.
- Inspect cached repository metadata.
- Build local dependency graphs.

Operations that need admin authorization:

- Install package.
- Remove package.
- Upgrade/downgrade package.
- Enable or disable repositories.
- Import repo GPG keys.
- Modify system DNF cache.
- Apply transaction history undo/redo.

Future write actions should use D-Bus/Polkit through DNF5 daemon. The GUI should request authorization only for the exact action, not run the entire process as root.

## First Version Features

### Search

Input types:

- Package name: `firefox`
- Full NEVRA if pasted.
- Binary name: `bash`
- Absolute path: `/usr/bin/bash`
- Library/capability: `libgtk-4.so.1`
- RPM capability: `python(abi) = 3.13`

Search results should show:

- Package name.
- Version/release/arch.
- Installed or available.
- Summary.
- Repository.
- Match reason: exact package, owns file, provides capability, binary match, available package.

### Package Overview

Fields:

- Name.
- Epoch/version/release/arch.
- Summary.
- Description.
- Installed size.
- License.
- URL.
- Source RPM.
- Vendor.
- Packager.
- Repository installed from.
- Install reason.
- Install time if known.
- Latest transaction if known.

Plain-language computed labels:

- User installed.
- Installed as dependency.
- Third-party repo.
- Fedora repo.
- Extra package.
- Leaf package.
- Update available.
- System-sensitive package.

### Dependencies Tab

Separate dependency groups:

- Requires.
- Requires-pre/scriptlet requirements.
- Recommends.
- Suggests.
- Supplements.
- Enhances.
- Provides.
- Conflicts.
- Obsoletes.

Each row should show:

- Raw capability.
- Relationship type.
- Resolved installed provider.
- Best available provider.
- Provider repository.
- Whether the provider is installed.

### Reverse Dependencies Tab

Views:

- Direct hard reverse dependencies.
- Direct weak reverse dependencies.
- Recursive reverse dependency tree.
- User-installed roots that depend on this package.

Useful filters:

- Installed only.
- Hard dependencies only.
- Include weak dependencies.
- Hide system packages.
- Depth limit.

### Files Tab

List package-owned files grouped by type:

- Executables.
- Libraries.
- Config files.
- Systemd units.
- Desktop entries.
- Documentation.
- Licenses.
- Icons.
- Other files.

Actions:

- Copy path.
- Open containing folder.
- Search within package files.
- Resolve selected file back to owner package.

### History Tab

Show:

- Transaction that installed the package.
- Command line if available.
- Packages installed in the same transaction.
- Updates involving the package.
- Previous versions recorded by DNF history.
- Whether the package appears user-requested or dependency-installed.

### Repository Tab

Show:

- Installed-from repo ID.
- Currently enabled repos containing the package.
- Whether the installed package is still available.
- Whether it is an extra.
- Whether a newer version is available.
- Vendor changes.
- Source RPM and repo relationship.

### Impact Tab

Read-only removal understanding:

- Is this package a leaf?
- Is it listed by autoremove?
- What installed packages directly require it?
- What user-installed packages may be affected?
- What DNF would remove if simulation support is implemented.

Important: removal impact must not be guessed only from static dependencies. Use DNF/libdnf5 transaction solving or DNF5 daemon simulation when available.

## Main Window Layout

Recommended first layout:

- Top toolbar:
  - Search field.
  - Search type indicator.
  - Refresh metadata button.
  - Settings button.
- Left pane:
  - Search results.
  - Recent packages.
  - Quick filters: leaves, extras, large packages, autoremove candidates.
- Main pane:
  - Package title/header.
  - Status badges.
  - Tab widget for Overview, Dependencies, Reverse Dependencies, Files, History, Repository, Impact.
- Bottom/status area:
  - Current backend state.
  - Last query command/API call.
  - Warnings/errors.

Keep the first UI dense and practical. Tables and trees should be first-class controls, not hidden behind a decorative graph.

## Suggested Source Tree

```text
dnf-helper/
  CMakeLists.txt
  README.md
  src/
    main.cpp
    app/
      Application.cpp
      Application.h
      Settings.cpp
      Settings.h
    ui/
      MainWindow.cpp
      MainWindow.h
      SearchPanel.cpp
      SearchPanel.h
      PackageHeaderWidget.cpp
      PackageHeaderWidget.h
      OverviewTab.cpp
      OverviewTab.h
      DependenciesTab.cpp
      DependenciesTab.h
      ReverseDependenciesTab.cpp
      ReverseDependenciesTab.h
      FilesTab.cpp
      FilesTab.h
      HistoryTab.cpp
      HistoryTab.h
      RepositoryTab.cpp
      RepositoryTab.h
      ImpactTab.cpp
      ImpactTab.h
    model/
      Package.cpp
      Package.h
      Dependency.cpp
      Dependency.h
      Transaction.cpp
      Transaction.h
      Repository.cpp
      Repository.h
      PackageQueryResult.cpp
      PackageQueryResult.h
    backend/
      PackageBackend.h
      Dnf5CliBackend.cpp
      Dnf5CliBackend.h
      Dnf5Process.cpp
      Dnf5Process.h
      RpmDatabaseBackend.cpp
      RpmDatabaseBackend.h
      Dnf5DaemonClient.cpp
      Dnf5DaemonClient.h
    services/
      PackageResolver.cpp
      PackageResolver.h
      DependencyResolver.cpp
      DependencyResolver.h
      PackageClassifier.cpp
      PackageClassifier.h
      ReportExporter.cpp
      ReportExporter.h
    util/
      HumanSize.cpp
      HumanSize.h
      CommandFormatter.cpp
      CommandFormatter.h
      Error.cpp
      Error.h
  data/
    org.example.DnfHelper.desktop
    org.example.DnfHelper.metainfo.xml
    icons/
  tests/
    PackageClassifierTest.cpp
    DependencyParserTest.cpp
```

## Core Classes

### `PackageBackend`

Abstract interface for package data.

Responsibilities:

- Search packages.
- Load package details.
- Load dependencies.
- Load reverse dependencies.
- Load package file list.
- Load history.
- Load repository facts.
- Run removal simulation if supported.

This allows the app to start with CLI-backed queries and later move to libdnf5 without rewriting the UI.

### `Dnf5CliBackend`

Phase 1 backend using `QProcess`.

Responsibilities:

- Execute DNF5 commands.
- Prefer JSON output where DNF5 supports it.
- Keep command construction centralized.
- Parse results into app models.
- Return structured errors.
- Expose the exact command for debug display.

Avoid spreading subprocess calls across UI widgets.

### `PackageResolver`

Turns user input into package candidates.

Resolution order:

1. Absolute file path owner.
2. Exact installed package.
3. Exact available package.
4. Capability provider.
5. Binary name search.
6. Fuzzy package search.

### `DependencyResolver`

Builds dependency and reverse dependency views.

Responsibilities:

- Keep hard and weak dependencies separate.
- Resolve capabilities to provider packages.
- Build recursive reverse dependency trees.
- Apply depth limits.
- Detect cycles.

### `PackageClassifier`

Adds user-facing labels.

Examples:

- `UserInstalled`
- `DependencyInstalled`
- `Leaf`
- `Extra`
- `AutoremoveCandidate`
- `ThirdPartyRepository`
- `SystemSensitive`
- `UpdateAvailable`

This logic should be testable and separate from widgets.

## Data Model Sketch

```cpp
struct PackageId {
    QString name;
    QString epoch;
    QString version;
    QString release;
    QString arch;
};

struct Package {
    PackageId id;
    QString summary;
    QString description;
    QString license;
    QString url;
    QString sourceRpm;
    QString vendor;
    QString packager;
    QString repoId;
    bool installed = false;
    qint64 installSize = 0;
    QDateTime installTime;
};

enum class DependencyType {
    Requires,
    RequiresPre,
    Recommends,
    Suggests,
    Supplements,
    Enhances,
    Provides,
    Conflicts,
    Obsoletes
};

struct DependencyEdge {
    PackageId from;
    QString capability;
    DependencyType type;
    std::optional<PackageId> installedProvider;
    QList<PackageId> availableProviders;
};
```

## DNF5 Command Prototype Map

Useful commands for early backend work:

```text
dnf5 repoquery --installed <package>
dnf5 info --installed <package>
dnf5 repoquery --requires <package>
dnf5 repoquery --recommends <package>
dnf5 repoquery --suggests <package>
dnf5 repoquery --provides <package>
dnf5 repoquery --whatrequires <capability>
dnf5 repoquery --whatdepends <capability>
dnf5 repoquery --recursive --whatrequires <capability>
dnf5 repoquery --leaves --installed
dnf5 repoquery --extras --installed
dnf5 info --autoremove
dnf5 history list --json
dnf5 history info <transaction-id> --json
dnf5 repo list
dnf5 repo info
rpm -qf <file-path>
rpm -ql <package>
```

Use `rpm` directly for file ownership and file listing if it is simpler and more stable than DNF output.

## Error Handling

Common states the UI should handle clearly:

- Package not found.
- Multiple packages match.
- Package is installed but repo metadata is missing.
- DNF cache is stale.
- Network unavailable.
- DNF5 command missing.
- DNF history unavailable or permission-limited.
- Backend command timed out.
- Query cancelled by user.

All backend calls should return structured errors, not raw text blobs.

## Performance Plan

- Run package queries off the UI thread.
- Use `QFuture`, `QThreadPool`, or worker `QObject` instances in dedicated threads.
- Cancel stale searches when the user types a new query.
- Cache package summaries for the session.
- Lazy-load heavy tabs only when opened.
- Put depth limits on recursive reverse dependencies.
- Add progress indicators for expensive queries.

## Settings

Useful settings:

- Installed packages only by default.
- Include available packages in search.
- Include weak dependencies in reverse dependency view.
- Max recursive dependency depth.
- Show raw RPM capabilities.
- Show debug command panel.
- Use cached metadata only.
- Allow network metadata refresh.
- Enable experimental removal simulation.

## Testing Plan

Unit tests:

- Package ID/NEVRA parsing.
- Dependency type mapping.
- Capability/provider mapping.
- Package classification.
- Human-readable size formatting.
- Error mapping from backend failures.

Integration tests:

- Backend command construction.
- Parsing JSON DNF history output.
- Parsing sample DNF/RPM outputs.

Manual Fedora tests:

- Core package such as `glibc`.
- Desktop app such as `firefox`.
- Leaf app package.
- Library package with many reverse dependencies.
- Third-party repo package.
- Local RPM package.
- Extra package no longer in enabled repos.

## Milestones

### Milestone 1: Read-Only CLI Prototype

- CMake Qt6 app opens.
- Search by installed package name.
- Show overview metadata.
- Show dependencies as raw capabilities.
- No privileged operations.

### Milestone 2: Useful Troubleshooting Tool

- Resolve file paths to owner package.
- Show package file list.
- Show reverse dependencies.
- Show leaf and extra package browsers.
- Add status badges.

### Milestone 3: Clear Explanations

- Resolve capabilities to provider packages.
- Show "why installed" information.
- Add history tab.
- Add repository/trust tab.
- Add Markdown/JSON export.

### Milestone 4: Impact Analysis

- Add autoremove candidate view.
- Add read-only removal impact simulation if supported.
- Highlight user-installed and system-sensitive packages in impact results.

### Milestone 5: Optional Admin Actions

- Add DNF5 daemon client.
- Add Polkit-backed install/remove/update actions.
- Keep all admin actions explicit and confirmable.

## Open Questions

- Is the app targeting GNOME, KDE, or both equally?
- Should the first UI use pure Qt Widgets or KDE Frameworks components?
- Is libdnf5 C++ API stable enough for the target Fedora releases?
- Which DNF5 commands provide reliable JSON output beyond history?
- Should the app support only Fedora Workstation initially, or also Silverblue/Kinoite-style systems?
- Should package mutation features be excluded permanently to keep the app a safe inspection tool?

## Recommended First Implementation Choice

Start with a Qt6 Widgets app using a `PackageBackend` abstraction and a `Dnf5CliBackend`.

That gives a fast path to a working Fedora utility while keeping the backend replaceable. Once the query behavior is proven, move high-value calls to libdnf5 or DNF5 daemon APIs where they are stable and documented enough.
