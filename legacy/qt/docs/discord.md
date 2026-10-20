Qt6 GUI for Fedora that helps users understand installed RPM packages without having to memorise DNF/RPM commands. Search for a package name, partial name, command, capability, or file path, then inspect what the package is, what it requires, what requires it, what files it owns, related config, repo origin, history, and basic removal-impact clues.

For: Fedora users who want a clearer picture of installed RPMs, dependencies, related configs, and package impact.
Status: WIP / early read-only prototype
Compatible: Fedora + DNF5 + RPM
https://github.com/linuxgamerlife/lgl-dnf-helper
Feedback: bugs, confusing results, missing package relationships, UI improvements

Current version: v0.1.0. It is read-only and does not install, remove, update, or modify packages. Do not run it with sudo.

Impact tab: gives a simple package map, not a DNF remove simulation. Use it to understand risk, not as proof something is safe to remove.

Config: package-owned system config is shown from RPM metadata, user config is guessed from XDG paths because RPM does not track files in your home folder, and direct dependency config is included where relevant.

Performance: tabs are lazy-loaded to avoid running every DNF/RPM query on each search.

Tested on Fedora with DNF5. Still WIP, so expect rough edges and please report packages where the dependency/config picture looks wrong.
