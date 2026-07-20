# Fedora DNF Helper GUI Ideas

## Goal

Build a Fedora-compatible GUI helper that explains installed RPM packages in plain language. The core flow should be:

1. User enters a package name, binary name, library, or file path.
2. The app resolves that input to the installed RPM package or matching available package.
3. The app shows what the package is, why it is installed, what it requires, what requires it, and what would likely happen if it were removed.

The app should start as an inspection and understanding tool, not as a package manager replacement.

## DNF5 Capabilities To Build Around

DNF5 has useful query primitives for this product:

- `dnf5 repoquery --installed <package>`: resolve installed packages.
- `dnf5 info --installed <package>`: show package metadata.
- `dnf5 repoquery --requires <package>`: show capabilities required by a package.
- `dnf5 repoquery --recommends <package>` / `--suggests`: show weaker dependency relationships.
- `dnf5 repoquery --whatrequires <capability>`: show packages requiring a capability.
- `dnf5 repoquery --whatdepends <capability>`: include requires, recommends, suggests, enhances, and supplements.
- `dnf5 repoquery --providers-of=requires <package>`: resolve required capabilities to packages that provide them.
- `dnf5 repoquery --recursive --whatrequires <capability>`: show reverse dependency chains.
- `dnf5 repoquery --leaves --installed`: find installed packages not required by other installed packages.
- `dnf5 repoquery --extras --installed`: find installed packages no longer present in enabled repositories.
- `dnf5 info --autoremove`: show packages DNF believes are candidates for autoremove.
- `dnf5 history list --json` and `dnf5 history info <transaction> --json`: show where a package came from when history is available.
- `dnf5 repo list` / `dnf5 repo info`: explain repository origin and enabled repo state.

For implementation, prefer `libdnf5` or `python3-libdnf5` over parsing terminal output once the query model is understood. CLI calls are useful for prototyping and for proving exact DNF5 behavior.

## Sudo / User-Space Model

Most inspection features should not need sudo.

Safe without sudo:

- Reading installed RPM metadata.
- Querying installed packages.
- Showing dependencies and reverse dependencies.
- Showing package files.
- Showing package summaries, versions, licenses, URLs, vendors, packagers, source RPMs, and repo IDs.
- Reading DNF history if the system allows it.
- Reading enabled repository metadata already cached by DNF.
- Building a local dependency graph in the user's session.

May need network access but not sudo:

- Refreshing or loading remote repository metadata into a user cache, depending on how libdnf5 is configured.
- Searching available packages from enabled repos if metadata is stale or missing.

Needs admin authorization:

- Installing, removing, upgrading, downgrading, distro-syncing, or reinstalling packages.
- Enabling/disabling repos persistently.
- Importing or confirming repository GPG keys.
- Cleaning or changing system package caches.
- Running offline transactions.
- Undoing or redoing DNF history transactions.

Recommendation: keep the GUI process unprivileged. For mutating actions, use `dnf5daemon-server` with D-Bus and Polkit, or launch `dnf5` through a controlled privilege flow. Do not run the whole GUI with sudo.

## Main Screens

### Package Lookup

A focused search screen with one input:

- Package name: `firefox`
- Binary: `/usr/bin/firefox` or `firefox`
- Library: `libgtk-4.so.1`
- Capability: `python(abi) = 3.13`
- File path: `/usr/bin/bash`

Results should clearly distinguish:

- Exact installed match.
- Installed package that owns a file.
- Provider of a capability.
- Available but not installed package.
- No match.

### Package Overview

Show a compact summary:

- Name, epoch/version/release/arch.
- Installed size.
- Summary and description.
- Repository installed from.
- Reason installed: user-installed, dependency, weak dependency, group, or unknown.
- Source RPM.
- License.
- URL.
- Vendor and packager.
- Install date if available.
- Last transaction touching it if available.

Plain-language labels matter. For example, "Installed because another package needed it" is more useful than only showing `reason=dependency`.

### Dependency Explorer

Show dependency relationships in separate tabs or columns:

- Requires: hard runtime requirements.
- Requires-pre: scriptlet/pre-transaction requirements.
- Recommends: normally installed weak dependencies.
- Suggests: optional helpful packages.
- Supplements/enhances: packages pulled in because of another package or feature.
- Provides: capabilities this package satisfies.
- Conflicts and obsoletes: compatibility blockers.

For each capability, resolve the provider package when possible. Users should not have to understand that `libc.so.6(GLIBC_2.38)(64bit)` is provided by `glibc`.

### Reverse Dependencies

Show "What needs this?" in layers:

- Direct hard reverse dependencies.
- Direct weak reverse dependencies.
- Recursive reverse dependency tree.
- User-installed packages that eventually depend on it.
- System-critical packages in the chain.

This screen should answer: "Can I remove this without breaking something I care about?"

### Removal Impact Preview

Start read-only and simulation-first:

- Show packages DNF would remove with this package.
- Highlight user-installed packages in the removal set.
- Highlight desktop/session/kernel/system packages.
- Explain whether the target is a leaf package.
- Show whether `dnf5 autoremove` already considers it removable.
- Show alternatives: disable service, remove plugin, remove weak dependency only, or leave installed.

This feature probably needs DNF transaction solving. It should be implemented through libdnf5 goal solving or dnf5daemon rather than guessing from static dependency edges.

### Files View

List files owned by the package:

- Binaries.
- Libraries.
- Systemd units.
- Desktop entries.
- Config files.
- Documentation.
- Licenses.
- Man pages.
- Icons/appstream metadata.

Useful extras:

- "Open containing folder" for desktop use.
- "Copy path".
- "Which package owns this file?" reverse lookup.

### History View

Show:

- When it was installed.
- Which transaction installed it.
- What else was installed at the same time.
- Whether it was installed directly by the user or pulled in.
- Updates applied since installation.
- Previous versions if available in history.

DNF5 history has JSON output, so this can be made robust without terminal scraping.

### Repository And Trust View

Show:

- Installed-from repo ID.
- Current repo availability.
- Whether the package is an extra.
- Whether the package is obsoleted.
- Update availability.
- GPG/repo trust status where available.
- Vendor changes across updates.

This helps users understand third-party RPMs, COPR packages, local installs, and packages that may no longer be maintained.

## Useful Features Beyond The Core

- Natural-language explanations: "This package is a shared library used by 14 installed applications."
- "Why is this installed?" tracing from the package back to user-installed roots.
- "What installed this?" using DNF history.
- Dependency graph visualization with filters for hard, weak, recursive, installed-only, and available packages.
- Risk labels for package categories: kernel, bootloader, desktop shell, display manager, networking, package manager, core library.
- Leaf package browser: good candidates to inspect or remove.
- Extras browser: packages installed but absent from enabled repos.
- Duplicate package browser.
- Orphan/autoremove browser.
- Large package browser sorted by installed size.
- Repo browser grouped by Fedora, updates, updates-testing, COPR, RPM Fusion, local, and unknown.
- File ownership lookup.
- CLI command preview: show the exact `dnf5` query behind each result for transparency.
- Export report as Markdown or JSON for support requests.
- "Compare two packages" for forks, compat packages, and alternatives.
- "Explain capability" helper for RPM dependency strings.
- Offline mode that uses only installed RPM DB and cached metadata.
- Support bundle mode: collect package metadata, repo IDs, and history snippets without private user files.

## UI Shape

The first version should be a dense, practical desktop tool:

- Top search bar.
- Left results/sidebar for matched packages.
- Main package detail pane.
- Tabs for Overview, Dependencies, Reverse Dependencies, Files, History, Repositories, and Impact.
- Clear badges for installed, available, extra, leaf, user-installed, dependency, update available, third-party repo, and system-critical.
- Dependency graph as an optional view, not the only view.
- Every technical RPM term should have a short explanation tooltip.

Avoid making the graph the whole product. A table-first view is faster for real troubleshooting.

## Architecture Ideas

Good starting stack options:

- Python + GTK4/libadwaita + `python3-libdnf5`.
- Rust + GTK4/libadwaita + subprocess-backed DNF5 prototype first, then libdnf5 bindings if available and practical.
- Qt/KDE stack if targeting Plasma users first.

Recommended architecture:

- GUI frontend stays unprivileged.
- Query service module wraps libdnf5 queries.
- Transaction preview module uses libdnf5 goal solving or dnf5daemon.
- Privileged action module is separate and only used for explicit install/remove/update flows.
- Cache normalized package facts in memory for fast navigation.
- Keep a "raw facts" panel or debug export for troubleshooting.

Prototype order:

1. CLI prototype that emits structured JSON for one package.
2. GUI package overview and dependencies.
3. Reverse dependency and leaf/orphan views.
4. Removal simulation.
5. History and repo trust details.
6. Optional privileged actions through dnf5daemon/Polkit.

## Data Model Sketch

Package:

- name
- epoch
- version
- release
- arch
- full_nevra
- summary
- description
- installed
- reason
- repo_id
- source_rpm
- license
- vendor
- packager
- url
- install_size
- install_time

Dependency edge:

- from_package
- to_capability
- resolved_provider_package
- relation_type: requires, requires_pre, recommends, suggests, supplements, enhances, conflicts, obsoletes, provides
- strength: hard, weak, reverse, informational
- installed_provider

Transaction:

- id
- timestamp
- command_line
- altered_packages
- reason
- user_id if available and appropriate to show

## Important Product Warnings

- Static dependency graphs are not the same as DNF transaction results. Always use DNF/libdnf solving for "what will be removed?"
- Weak dependencies can confuse users. Keep hard dependencies visually separate from recommends/suggests.
- Capabilities are not always package names. Resolve providers wherever possible.
- Recursive reverse dependency output can become huge. Provide depth limits and filters.
- Package names can be ambiguous across architectures and repositories. Show NEVRA when precision matters.
- System-critical labels should be conservative and explainable.
- Do not encourage removing core packages just because they appear as leaves.

## Packaging And Fedora Fit

- Package as an RPM.
- Depend on DNF5/libdnf5 packages already shipped by Fedora.
- Use desktop integration: `.desktop` file, AppStream metadata, symbolic icon.
- Follow Fedora packaging guidelines and avoid bundling system libraries.
- Make the app useful without network access.
- Keep all destructive actions behind a confirmation and Polkit/admin authorization.

## Sources Checked

- DNF5 repoquery command documentation: https://dnf5.readthedocs.io/en/latest/commands/repoquery.8.html
- DNF5 info command documentation: https://dnf5.readthedocs.io/en/stable/commands/info.8.html
- DNF5 history command documentation: https://dnf5.readthedocs.io/en/latest/commands/history.8.html
- DNF5 daemon documentation: https://dnf5.readthedocs.io/en/latest/dnf_daemon/index.html
- DNF5 D-Bus API notes: https://dnf5.readthedocs.io/en/latest/dnf_daemon/dnf5daemon_dbus_api.8.html
- DNF5/libdnf5 overview: https://dnf5.readthedocs.io/en/latest/about.html
- Fedora `python3-libdnf5` package page: https://packages.fedoraproject.org/pkgs/dnf5/python3-libdnf5/
- Fedora `dnf5daemon-server-polkit` package page: https://packages.fedoraproject.org/pkgs/dnf5/dnf5daemon-server-polkit/
