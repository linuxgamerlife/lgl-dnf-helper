# LGL App Style Guide For DNF Helper

This guide keeps the DNF Helper app visually and behaviorally consistent with `lgl-scxctl-manager` while adapting the patterns to a DNF/RPM inspection tool.

Reference app checked:

`/home/lgl/projects/scxctl-manager/Version 1.0.1/lgl-scxctl-manager/`

## Overall Direction

DNF Helper should feel like a compact Linux system utility:

- Practical before decorative.
- Qt6 Widgets first.
- System theme by default.
- Dense tabs and tables.
- Clear status indicators.
- Plain-language explanations beside technical package data.
- No full-window marketing screen.
- No custom global stylesheet unless the system theme fails a specific usability need.

The reference app explicitly uses the system Qt theme and avoids a custom stylesheet. Follow that unless a DNF-specific view needs local styling for readability.

## Technology Baseline

Match the reference app's baseline:

- CMake project.
- C++20.
- Qt6 `Core` and `Widgets`.
- `CMAKE_AUTOMOC`, `CMAKE_AUTOUIC`, and `CMAKE_AUTORCC` enabled.
- Warnings enabled for GCC/Clang: `-Wall -Wextra -Wpedantic`.
- Resource file for the app icon.
- Fedora RPM packaging with desktop entry, AppStream metadata, icon install, and cache refresh scriptlets.

Suggested project identity:

- Binary: `lgl-dnf-helper`
- Display name: `DNF Helper`
- AppStream ID: `com.linuxgamerlife.lgl-dnf-helper`
- Desktop file: `lgl-dnf-helper.desktop`
- Icon: `lgl-dnf-helper.png`

## Window Shape

Use a similar default size and density:

- Minimum size around `720x560`.
- Initial size around `860x640` because package tables need more width than the scheduler app.
- Root layout margins: `12, 12, 12, 8`.
- Root spacing: `8`.
- Header frame at top.
- Main `QTabWidget` underneath.
- `QStatusBar` for short operation state.

Reference pattern:

- Header title on the left.
- Current state indicator on the right.
- Refresh button at the far right.

DNF Helper header:

- Title: `DNF Helper`
- Subtitle is optional; keep it out of the header if space is tight.
- Status dot/label: `Ready`, `Querying`, `Cache stale`, `Offline`, or `Error`.
- Refresh button: `Refresh Metadata` or `Refresh`.

## Navigation

Use tabs as the main navigation, matching the reference app.

Recommended tabs:

- `Overview`
- `Dependencies`
- `Required By`
- `Files`
- `History`
- `Repository`
- `Impact`
- `Log`

Optional browser tabs:

- `Leaves`
- `Extras`
- `Large Packages`

Keep tab labels short. Avoid verbose labels like `Reverse Dependencies`; use `Required By` in the UI and explain it in tooltips or detail text.

## Layout Patterns

Use the same practical Qt Widgets patterns:

- `QVBoxLayout` for tab roots.
- `QGridLayout` inside `QGroupBox` for key/value facts.
- `QHBoxLayout` for button rows and filters.
- `QTableWidget` or model/view tables for dense references.
- `QTextEdit` or `QPlainTextEdit` for logs and raw command output.
- `QComboBox` for filters.
- `QLineEdit` for search and optional command/capability input.
- `QCheckBox` for include/exclude toggles.

Spacing and margins:

- Main root margins: `12`.
- Tab margins: `10` or `12`.
- Group/grid spacing: `8`.
- Button height: `34-36`.
- Header margins: `12, 8, 12, 8`.

## Visual Style

Use system theme colors as the default.

Allowed local color usage:

- Green: success/installed/running/ok.
- Red: error/not found/danger.
- Orange: warning/stale/third-party/extra.
- Gray: inactive/unknown.

Avoid a large custom palette. The reference app uses color sparingly for status dots, tray icon state, log lines, and table emphasis.

Status indicators:

- Use a colored dot label, matching the reference pattern: `●`.
- Use tooltips to explain state.
- Keep state text beside the dot.

Badges:

- Use compact labels in the package header.
- Examples: `Installed`, `User installed`, `Dependency`, `Leaf`, `Extra`, `Third-party`, `Update available`.
- Prefer system-style labels or simple `QLabel` chips with restrained local styling.

Icons:

- The reference app uses Unicode symbols in buttons and labels.
- For consistency, Unicode symbols are acceptable: `↻`, `▶`, `⏹`, `⇄`, `⊞`, `⚠`.
- Do not overuse symbols. For package data, text clarity matters more.

## Text Style

Use direct, plain labels:

- `Package:`
- `Version:`
- `Repository:`
- `Install reason:`
- `Required by:`
- `Files:`
- `Installed size:`

Prefer user-facing explanations where RPM terms are obscure:

- `Required by` instead of only `whatrequires`.
- `Hard requirement` instead of only `requires`.
- `Optional helper package` instead of only `suggests`.
- `Normally installed weak dependency` for `recommends`.

Use rich text sparingly for setup/about screens, as in the reference app.

Keep setup messages helpful and specific:

- Explain what command or package is missing.
- Show Fedora install commands.
- Explain which features still work without it.

## Setup / Missing Dependency Mode

Mirror the reference app's setup mode pattern.

If required tools are missing:

- Insert a `Setup` tab at index `0`.
- Select it automatically.
- Disable tabs that cannot work.
- Keep tabs enabled if they can work partially.
- Show status bar message.

DNF Helper setup checks:

- `dnf5` available.
- `rpm` available.
- Optional: `dnf5daemon-server` available for future admin actions.
- Optional: Polkit agent available if privileged actions are enabled.

Suggested setup tab title:

- `⚠ DNF5 not detected`

Suggested behavior:

- If `dnf5` is missing but `rpm` exists, allow file/package ownership views that only need RPM.
- If both are missing, show setup guidance and disable query tabs.

## Privilege Model

Do not run the GUI as root.

Follow the reference pattern:

- Read-only commands run directly.
- Privileged actions use `pkexec` or, preferably for DNF5, D-Bus/Polkit through `dnf5daemon-server`.
- Handle authorization cancellation as a normal outcome, not a crash.
- Show short status bar feedback.
- Log the operation and result.

For the first DNF Helper version, keep the app read-only. Add privileged actions only after the inspection workflow is reliable.

## Command Execution UX

The reference app uses `QProcess`, status bar messages, and a Log tab. Use the same behavior.

DNF Helper should:

- Run DNF/RPM commands off the UI thread or through asynchronous `QProcess`.
- Show `Querying package...` in the status bar.
- Append timestamped command summaries to the Log tab.
- Capture stderr and show it in red.
- Treat expected empty results as normal states.
- Avoid log spam during repeated refreshes.
- Keep a debug option to show the exact backend command.

Use structured parsing where available. For DNF5 history, prefer JSON output.

## Tables

The reference app uses tables heavily for scheduler and flags reference. DNF Helper should do the same for package facts.

Table defaults:

- Row selection.
- No editing.
- Alternating row colors.
- No visible grid.
- Word wrap enabled for descriptive columns.
- Vertical header hidden.
- Resize rows to contents after population.
- Monospace font for package names, capabilities, file paths, and commands.

Recommended table columns:

Dependencies:

- `Type`
- `Capability`
- `Installed Provider`
- `Available Provider`
- `Repository`

Required By:

- `Package`
- `Reason`
- `Installed Reason`
- `Repository`
- `Risk`

Files:

- `Type`
- `Path`

History:

- `Transaction`
- `Date`
- `Action`
- `Version`
- `Command`

Repository:

- `Repo`
- `Status`
- `Version`
- `Vendor`

## Logging

Keep a dedicated `Log` tab.

Log format:

- Prefix entries with local time: `[hh:mm:ss]`.
- Green for successful command/output.
- Orange for warnings and fallbacks.
- Red for errors.
- Normal palette text for neutral entries.

Do not expose overwhelming raw output by default. Summarize normal command output in the UI, and keep raw details in the Log tab or export report.

## System Tray

The reference app has a tray because it controls a long-running scheduler service. DNF Helper probably does not need a tray in version 1.

Use a tray only if future features add:

- Background package monitoring.
- Update/repo status notifications.
- Long-running transaction monitoring.

If a tray is added later, follow the reference pattern:

- Colored state icon.
- Disabled status action at top.
- Separator.
- Show Window.
- Quit.
- Double-click toggles visibility.
- Closing the window hides to tray only if the tray is enabled.

## About Dialog

Use a `QMessageBox` with rich text, matching the reference app.

Include:

- App name.
- Version.
- One-sentence purpose.
- Created by LinuxGamerLife and helper credit if desired.
- Website/GitHub/YouTube/social links.
- Upstream references: Fedora, DNF5, RPM.
- License.

Keep the text shorter than the reference if the app has no external runtime service to explain.

## Packaging Style

Follow the reference packaging structure:

```text
packaging/
  com.linuxgamerlife.lgl-dnf-helper.metainfo.xml
  lgl-dnf-helper.spec
  lgl-dnf-helper_icon.png
```

Install:

- Binary to `%{_bindir}`.
- Desktop entry to `%{_datadir}/applications`.
- Icon to hicolor `256x256/apps`.
- Pixmaps fallback.
- AppStream metainfo to `%{_datadir}/metainfo`.

Post-install cache refreshes:

- `gtk-update-icon-cache`
- `update-desktop-database`
- `appstreamcli refresh`
- `kbuildsycoca6` where available

Runtime dependencies:

- `qt6-qtbase`
- `dnf5`
- `rpm`
- Optional/recommended: `dnf5daemon-server`
- Optional/recommended: `polkit`

## Desktop Entry Style

Use the same category style:

```ini
[Desktop Entry]
Type=Application
Name=DNF Helper
GenericName=Package Inspector
Comment=Inspect installed RPM packages and DNF dependencies
Exec=lgl-dnf-helper
Icon=lgl-dnf-helper
Terminal=false
StartupNotify=true
Categories=System;Settings;
Keywords=dnf;dnf5;rpm;package;dependencies;fedora;repository;
X-KDE-SubstituteUID=false
```

## AppStream Style

Use the same AppStream shape:

- Component type: `desktop-application`.
- ID under `com.linuxgamerlife`.
- Metadata and project license.
- Short summary.
- Description with 2-3 paragraphs.
- Launchable desktop ID.
- Homepage and bugtracker URLs.
- Developer ID/name.
- Categories: `System`, `Settings`.
- Keywords for DNF/RPM/Fedora.
- Controls: pointing and keyboard.
- Minimum display length around `640`.
- Release notes per version.

Mention that the app opens in setup mode if DNF5 is missing.

## Coding Style

Follow the reference app's pragmatic style:

- `MainWindow` owns top-level widgets.
- UI setup split into `setupUi()`, `setupMenuBar()`, `setupConnections()`, and optional setup/tray methods.
- Use private slots for user actions.
- Use helper methods for repeated UI rows.
- Use `QProcess` asynchronously.
- Use lambdas for local signal handling, but do not capture stack variables by reference in async callbacks.
- Validate user-provided command fragments before passing them to tools.
- Keep privileged commands centralized.

For DNF Helper, split backend logic earlier than the reference app because package querying will grow:

- `PackageBackend`
- `Dnf5CliBackend`
- `PackageResolver`
- `DependencyResolver`
- `PackageClassifier`

The UI should not construct raw DNF commands directly.

## DNF Helper-Specific Style Rules

Package header:

- Show package name prominently.
- Show NEVRA in monospace.
- Show badges directly under or beside the title.
- Show repository and install reason near the top.

Dependency views:

- Keep hard and weak dependencies separate.
- Do not mix `requires` and `recommends` without labels.
- Resolve capabilities to provider packages whenever possible.
- Show raw capability text, but do not make it the only explanation.

Impact view:

- Use warning colors conservatively.
- Clearly state when a result is static analysis vs DNF transaction simulation.
- Never imply a package is safe to remove just because it is a leaf.

Repository view:

- Call out third-party repos, local packages, extras, and vendor changes.
- Use plain terms before technical terms.

## Initial UI Checklist

Before implementing the first screen, check that it follows the LGL app style:

- Uses Qt6 Widgets.
- Uses system theme.
- Has a compact header bar.
- Has a status dot and status label.
- Uses tabs for major views.
- Uses group boxes for key facts.
- Uses tables for dependency/file/history data.
- Has a Log tab.
- Has setup mode for missing tools.
- Does not run the GUI as root.
- Keeps command execution centralized.
- Has desktop/AppStream/RPM metadata planned.

## Differences From SCX Manager

DNF Helper should intentionally differ in a few places:

- No system tray in version 1 unless background monitoring is added.
- Wider default window for package tables.
- Backend classes should be separated from `MainWindow` from the start.
- Privileged actions should be deferred; SCX Manager controls a service, but DNF Helper is primarily an inspector.
- Graph views should be optional, while tables remain the main interface.

These differences keep the app consistent in style without copying behavior that only makes sense for a scheduler controller.
