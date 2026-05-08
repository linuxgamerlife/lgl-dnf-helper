#include "Dnf5CliBackend.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QSet>

#include "Dnf5Process.h"

Dnf5CliBackend::Dnf5CliBackend(QObject *parent) : PackageBackend(parent) {}

void Dnf5CliBackend::checkTools() {
    emit toolsChecked(!QStandardPaths::findExecutable("dnf5").isEmpty(),
                      !QStandardPaths::findExecutable("rpm").isEmpty());
}

void Dnf5CliBackend::search(const QString &query) {
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        emit searchCompleted({});
        return;
    }

    if (trimmed.startsWith('/')) {
        runCommand("rpm", {"-qf", "--qf", "%{NAME}|%{EPOCH}|%{VERSION}|%{RELEASE}|%{ARCH}|@System|||||%{SUMMARY}|%{LICENSE}|%{URL}|%{SOURCERPM}|%{VENDOR}|%{PACKAGER}\\n", trimmed},
                   [this, trimmed](int exitCode, const QString &out, const QString &err) {
            QList<PackageQueryResult> results;
            if (exitCode == 0) {
                for (const Package &packageBase : packagesFromDelimitedOutput(out)) {
                    Package package = packageBase;
                    package.installed = true;
                    results << PackageQueryResult{package, PackageQueryResult::MatchKind::FileOwner, trimmed};
                }
            } else if (!err.trimmed().isEmpty()) {
                emit errorOccurred(err.trimmed());
            }
            emit searchCompleted(results);
        });
        return;
    }

    runCommand("dnf5", {"repoquery", "--installed", "--queryformat=" + packageQueryFormat(), trimmed},
               [this, trimmed](int exitCode, const QString &out, const QString &err) {
        QList<PackageQueryResult> results;
        QSet<QString> seenResults;
        if (exitCode == 0) {
            for (Package package : packagesFromDelimitedOutput(out)) {
                const QString key = package.id.nevra() + "|" + package.repoId + "|installed";
                if (seenResults.contains(key))
                    continue;
                seenResults.insert(key);
                package.installed = true;
                results << PackageQueryResult{package, PackageQueryResult::MatchKind::ExactPackage, trimmed};
            }
        }
        if (results.isEmpty()) {
            runCommand("dnf5", {"repoquery", "--installed", "--queryformat=" + packageQueryFormat(), "--whatprovides", trimmed},
                       [this, trimmed](int installedProviderExit, const QString &installedProviderOut, const QString &installedProviderErr) {
                QList<PackageQueryResult> providerResults;
                QSet<QString> seenProviderResults;
                if (installedProviderExit == 0) {
                    for (Package package : packagesFromDelimitedOutput(installedProviderOut)) {
                        package.installed = true;
                        const auto matchKind = package.id.name == trimmed
                            ? PackageQueryResult::MatchKind::ExactPackage
                            : PackageQueryResult::MatchKind::CapabilityProvider;
                        const QString key = package.id.nevra()
                            + "|" + package.repoId
                            + "|installed|"
                            + QString::number(static_cast<int>(matchKind));
                        if (seenProviderResults.contains(key))
                            continue;
                        seenProviderResults.insert(key);
                        providerResults << PackageQueryResult{package, matchKind, trimmed};
                    }
                } else if (!installedProviderErr.trimmed().isEmpty()) {
                    emit errorOccurred(installedProviderErr.trimmed());
                }

                runCommand("dnf5", {"repoquery", "--available", "--queryformat=" + packageQueryFormat(), "--whatprovides", trimmed},
                           [this, trimmed, providerResults, seenProviderResults](int providerExit, const QString &providerOut, const QString &providerErr) mutable {
                    if (providerExit == 0) {
                        for (Package package : packagesFromDelimitedOutput(providerOut)) {
                            package.installed = false;
                        const auto matchKind = package.id.name == trimmed
                            ? PackageQueryResult::MatchKind::AvailablePackage
                            : PackageQueryResult::MatchKind::CapabilityProvider;
                        const QString key = package.id.nevra()
                            + "|" + package.repoId
                            + "|available|"
                            + "|" + QString::number(static_cast<int>(matchKind));
                        if (seenProviderResults.contains(key))
                            continue;
                        seenProviderResults.insert(key);
                        providerResults << PackageQueryResult{package, matchKind, trimmed};
                    }
                    } else if (!providerErr.trimmed().isEmpty()) {
                        emit errorOccurred(providerErr.trimmed());
                    }

                    if (!providerResults.isEmpty()) {
                        emit searchCompleted(providerResults);
                        return;
                    }

                    const QString fuzzyQuery = "*" + trimmed + "*";
                    runCommand("dnf5", {"repoquery", "--installed", "--queryformat=" + packageQueryFormat(), fuzzyQuery},
                               [this, fuzzyQuery](int fuzzyInstalledExit, const QString &fuzzyInstalledOut, const QString &fuzzyInstalledErr) {
                        QList<PackageQueryResult> fuzzyResults;
                        QSet<QString> seenFuzzyResults;
                        if (fuzzyInstalledExit == 0) {
                            for (Package package : packagesFromDelimitedOutput(fuzzyInstalledOut)) {
                                package.installed = true;
                                const QString key = package.id.nevra() + "|installed";
                                if (seenFuzzyResults.contains(key))
                                    continue;
                                seenFuzzyResults.insert(key);
                                fuzzyResults << PackageQueryResult{package, PackageQueryResult::MatchKind::FuzzyPackage, fuzzyQuery};
                            }
                        } else if (!fuzzyInstalledErr.trimmed().isEmpty()) {
                            emit errorOccurred(fuzzyInstalledErr.trimmed());
                        }

                        runCommand("dnf5", {"repoquery", "--available", "--queryformat=" + packageQueryFormat(), fuzzyQuery},
                                   [this, fuzzyQuery, fuzzyResults, seenFuzzyResults](int fuzzyAvailableExit, const QString &fuzzyAvailableOut, const QString &fuzzyAvailableErr) mutable {
                            if (fuzzyAvailableExit == 0) {
                                for (Package package : packagesFromDelimitedOutput(fuzzyAvailableOut)) {
                                    package.installed = false;
                                    const QString key = package.id.nevra() + "|available";
                                    if (seenFuzzyResults.contains(key))
                                        continue;
                                    seenFuzzyResults.insert(key);
                                    fuzzyResults << PackageQueryResult{package, PackageQueryResult::MatchKind::FuzzyPackage, fuzzyQuery};
                                }
                            } else if (!fuzzyAvailableErr.trimmed().isEmpty()) {
                                emit errorOccurred(fuzzyAvailableErr.trimmed());
                            }
                            emit searchCompleted(fuzzyResults);
                        });
                    });
                });
            });
            return;
        }
        if (exitCode != 0 && !err.trimmed().isEmpty())
            emit errorOccurred(err.trimmed());
        emit searchCompleted(results);
    });
}

void Dnf5CliBackend::loadPackage(const QString &name) {
    runCommand("dnf5", {"repoquery", "--installed", "--queryformat=" + packageQueryFormat(), name},
               [this, name](int exitCode, const QString &out, const QString &err) {
        auto emitWithDescription = [this, name](Package package, bool installedOnly) {
            QStringList args{"repoquery"};
            if (installedOnly)
                args << "--installed";
            else
                args << "--available";
            args << "--queryformat=%{description}" << name;
            runCommand("dnf5", args, [this, name, package](int descriptionExit, const QString &descriptionOut, const QString &) mutable {
                if (descriptionExit == 0) {
                    QStringList lines = nonEmptyLines(descriptionOut);
                    lines.removeAll("Updating and loading repositories:");
                    lines.removeAll("Repositories loaded.");
                    package.description = lines.join('\n').trimmed();
                }
                emit packageLoaded(name, package);
            });
        };

        if (exitCode == 0) {
            const QList<Package> packages = packagesFromDelimitedOutput(out);
            if (!packages.isEmpty()) {
                emitWithDescription(packages.first(), true);
                return;
            }
        } else {
            if (!err.trimmed().isEmpty())
                emit errorOccurred(err.trimmed());
        }

        runCommand("dnf5", {"repoquery", "--available", "--queryformat=" + packageQueryFormat(), name},
                   [this, name, emitWithDescription](int availableExit, const QString &availableOut, const QString &availableErr) {
            if (availableExit == 0) {
                const QList<Package> packages = packagesFromDelimitedOutput(availableOut);
                if (!packages.isEmpty()) {
                    Package package = packages.first();
                    package.installed = false;
                    emitWithDescription(package, false);
                    return;
                }
            } else if (!availableErr.trimmed().isEmpty()) {
                emit errorOccurred(availableErr.trimmed());
            }

            Package package;
            package.id.name = name;
            package.installed = false;
            package.summary = "Package metadata not found";
            emit packageLoaded(name, package);
        });
    });
}

void Dnf5CliBackend::loadDependencies(const QString &name) {
    struct PendingDeps {
        int remaining = 0;
        QList<DependencyEdge> edges;
    };

    auto pending = QSharedPointer<PendingDeps>::create();
    const QList<QPair<DependencyType, QString>> queries = {
        {DependencyType::Requires, "--requires"},
        {DependencyType::RequiresPre, "--requires-pre"},
        {DependencyType::Recommends, "--recommends"},
        {DependencyType::Suggests, "--suggests"},
        {DependencyType::Supplements, "--supplements"},
        {DependencyType::Enhances, "--enhances"},
        {DependencyType::Provides, "--provides"},
        {DependencyType::Conflicts, "--conflicts"},
        {DependencyType::Obsoletes, "--obsoletes"},
    };
    pending->remaining = queries.size();

    for (const auto &query : queries) {
        runCommand("dnf5", {"repoquery", query.second, name},
                   [this, name, pending, type = query.first](int exitCode, const QString &out, const QString &err) {
            if (exitCode == 0) {
                pending->edges += dependencyEdgesFromLines(out, type, name);
            } else if (!err.trimmed().isEmpty()) {
                emit errorOccurred(err.trimmed());
            }

            --pending->remaining;
            if (pending->remaining == 0)
                emit dependenciesLoaded(name, pending->edges);
        });
    }
}

void Dnf5CliBackend::loadRequiredBy(const QString &name) {
    runCommand("dnf5", {"repoquery", "--installed", "--queryformat=" + packageQueryFormat(), "--whatdepends", name},
               [this, name](int exitCode, const QString &out, const QString &err) {
        if (exitCode == 0) {
            QList<DependencyEdge> edges;
            for (const Package &package : packagesFromDelimitedOutput(out)) {
                DependencyEdge edge;
                edge.from.name = name;
                edge.type = DependencyType::Requires;
                edge.capability = package.id.name;
                edge.installedProvider = package.id;
                edges << edge;
            }
            emit requiredByLoaded(name, edges);
        } else {
            if (!err.trimmed().isEmpty())
                emit errorOccurred(err.trimmed());
            emit requiredByLoaded(name, {});
        }
    });
}

void Dnf5CliBackend::loadFiles(const QString &name) {
    const QString rpmName = packageNameFromNevra(name);
    runCommand("rpm", {"-ql", rpmName}, [this, name](int exitCode, const QString &out, const QString &err) {
        if (exitCode == 0) {
            emit filesLoaded(name, nonEmptyLines(out));
        } else {
            if (!err.trimmed().isEmpty())
                emit errorOccurred(err.trimmed());
            emit filesLoaded(name, {});
        }
    });
}

void Dnf5CliBackend::loadHistory(const QString &name) {
    runCommand("dnf5", {"history", "list", "--contains-pkgs=" + name},
               [this, name](int exitCode, const QString &out, const QString &err) {
        if (exitCode == 0) {
            emit historyLoaded(name, historyRowsFromOutput(out, name));
        } else {
            if (!err.trimmed().isEmpty())
                emit errorOccurred(err.trimmed());
            emit historyLoaded(name, {});
        }
    });
}

void Dnf5CliBackend::loadRepositoryInfo(const QString &name) {
    struct PendingRepo {
        int remaining = 0;
        QList<QStringList> rows;
        Package installed;
        QList<Package> available;
        bool isExtra = false;
        bool hasUpgrade = false;
    };

    auto pending = QSharedPointer<PendingRepo>::create();
    pending->remaining = 4;

    auto finish = [this, pending, name]() {
        --pending->remaining;
        if (pending->remaining != 0)
            return;

        if (pending->installed.id.isValid()) {
            QString installedRepo = pending->installed.repoId;
            const QString vendor = pending->installed.vendor;

            // Cross-reference: when from_repo metadata is misleading (e.g., shows "terra" for
            // a package installed from RPM Fusion), look at available repos to find the real
            // source. Match by vendor since that's the most reliable origin indicator.
            if (!pending->available.isEmpty() && !vendor.isEmpty()) {
                for (const Package &avail : pending->available) {
                    if (avail.repoId != installedRepo && avail.vendor == vendor) {
                        // Available package matches vendor — use its repo as the correct origin
                        installedRepo = avail.repoId;
                        break;
                    }
                }
            }

            pending->rows << QStringList{installedRepo, "Installed from repo", pending->installed.id.evr(), vendor};
            pending->rows << QStringList{pending->installed.sourceRpm.isEmpty() ? "Unknown" : pending->installed.sourceRpm, "Source RPM", pending->installed.id.arch, repoKind(installedRepo)};
        }

        for (const Package &package : pending->available) {
            pending->rows << QStringList{package.repoId, "Available", package.id.evr(), package.vendor};
        }

        pending->rows << QStringList{"Extra package", pending->isExtra ? "Yes: not present in enabled repos" : "No", QString(), QString()};
        pending->rows << QStringList{"Update available", pending->hasUpgrade ? "Yes" : "No", QString(), QString()};
        emit repositoryInfoLoaded(name, pending->rows);
    };

    runCommand("dnf5", {"repoquery", "--installed", "--queryformat=" + packageQueryFormat(), name},
               [this, pending, finish](int exitCode, const QString &out, const QString &err) {
        if (exitCode == 0) {
            const QList<Package> packages = packagesFromDelimitedOutput(out);
            if (!packages.isEmpty())
                pending->installed = packages.first();
        } else if (!err.trimmed().isEmpty()) {
            emit errorOccurred(err.trimmed());
        }
        finish();
    });

    runCommand("dnf5", {"repoquery", "--available", "--queryformat=" + packageQueryFormat(), name},
               [this, pending, finish](int exitCode, const QString &out, const QString &err) {
        if (exitCode == 0)
            pending->available = packagesFromDelimitedOutput(out);
        else if (!err.trimmed().isEmpty())
            emit errorOccurred(err.trimmed());
        finish();
    });

    runCommand("dnf5", {"repoquery", "--extras", "--queryformat=" + packageQueryFormat(), name},
               [this, pending, finish](int exitCode, const QString &out, const QString &err) {
        if (exitCode == 0)
            pending->isExtra = !packagesFromDelimitedOutput(out).isEmpty();
        else if (!err.trimmed().isEmpty())
            emit errorOccurred(err.trimmed());
        finish();
    });

    runCommand("dnf5", {"repoquery", "--upgrades", "--queryformat=" + packageQueryFormat(), name},
               [this, pending, finish](int exitCode, const QString &out, const QString &err) {
        if (exitCode == 0)
            pending->hasUpgrade = !packagesFromDelimitedOutput(out).isEmpty();
        else if (!err.trimmed().isEmpty())
            emit errorOccurred(err.trimmed());
        finish();
    });
}

void Dnf5CliBackend::loadImpact(const QString &name) {
    struct PendingImpact {
        int remaining = 0;
        QList<QStringList> rows;
        int requiredByCount = 0;
        bool isLeaf = false;
        bool isAutoremove = false;
        bool isExtra = false;
    };

    auto pending = QSharedPointer<PendingImpact>::create();
    pending->remaining = 4;

    auto finish = [this, pending, name]() {
        --pending->remaining;
        if (pending->remaining != 0)
            return;

        pending->rows << QStringList{"What this means", "This is a read-only safety check. It helps you understand the package, but it is not a remove preview."};
        pending->rows << QStringList{"Other packages need it?", pending->requiredByCount == 0
            ? "No installed packages were found depending on it."
            : QString("Yes. %1 installed package(s) appear to depend on it. Check Required By before doing anything.").arg(pending->requiredByCount)};
        pending->rows << QStringList{"Leaf package?", pending->isLeaf
            ? "Yes. DNF says nothing installed needs it directly."
            : "No. DNF does not see it as a loose/standalone package."};
        pending->rows << QStringList{"Autoremove candidate?", pending->isAutoremove
            ? "Yes. DNF thinks it may no longer be needed."
            : "No. DNF does not currently mark it as unneeded."};
        pending->rows << QStringList{"Still in enabled repos?", pending->isExtra
            ? "No. It may be from a disabled repo, old upgrade, or local RPM."
            : "Yes. DNF can still find it in enabled repo metadata."};
        pending->rows << QStringList{"Simple advice", pending->requiredByCount == 0 && (pending->isLeaf || pending->isAutoremove)
            ? "Probably lower risk, but still run a real DNF remove preview before removing it."
            : "Be careful. Look at Required By, Files, and History before deciding what this package is for."};
        emit impactLoaded(name, pending->rows);
    };

    runCommand("dnf5", {"repoquery", "--installed", "--queryformat=%{name}", "--whatdepends", name},
               [this, pending, finish](int exitCode, const QString &out, const QString &err) {
        if (exitCode == 0)
            pending->requiredByCount = nonEmptyLines(out).size();
        else if (!err.trimmed().isEmpty())
            emit errorOccurred(err.trimmed());
        finish();
    });

    runCommand("dnf5", {"repoquery", "--leaves", "--queryformat=%{name}", name},
               [this, pending, finish](int exitCode, const QString &out, const QString &err) {
        if (exitCode == 0)
            pending->isLeaf = !nonEmptyLines(out).isEmpty();
        else if (!err.trimmed().isEmpty())
            emit errorOccurred(err.trimmed());
        finish();
    });

    runCommand("dnf5", {"repoquery", "--unneeded", "--queryformat=%{name}", name},
               [this, pending, name, finish](int exitCode, const QString &out, const QString &err) {
        if (exitCode == 0)
            pending->isAutoremove = nonEmptyLines(out).contains(name);
        else if (!err.trimmed().isEmpty())
            emit errorOccurred(err.trimmed());
        finish();
    });

    runCommand("dnf5", {"repoquery", "--extras", "--queryformat=%{name}", name},
               [this, pending, finish](int exitCode, const QString &out, const QString &err) {
        if (exitCode == 0)
            pending->isExtra = !nonEmptyLines(out).isEmpty();
        else if (!err.trimmed().isEmpty())
            emit errorOccurred(err.trimmed());
        finish();
    });
}

void Dnf5CliBackend::loadUserConfig(const QString &name) {
    struct PendingConfig {
        int remaining = 0;
        QList<QStringList> rows;
        QSet<QString> seenPaths;
    };

    const QString rpmName = packageNameFromNevra(name);
    runCommand("rpm", {"-ql", rpmName}, [this, name, rpmName](int exitCode, const QString &out, const QString &err) {
        if (exitCode == 0) {
            auto pending = QSharedPointer<PendingConfig>::create();
            pending->rows = userConfigRows(rpmName, nonEmptyLines(out));
            for (const QStringList &row : pending->rows) {
                if (row.size() > 1 && row[1].startsWith('/'))
                    pending->seenPaths.insert(row[1]);
            }
            pending->remaining = 1;

            auto finish = [this, pending, name]() {
                --pending->remaining;
                if (pending->remaining == 0)
                    emit userConfigLoaded(name, pending->rows);
            };

            runCommand("dnf5", {"repoquery", "--requires", rpmName}, [this, pending, rpmName, finish](int requiresExit, const QString &requiresOut, const QString &requiresErr) {
                if (requiresExit == 0) {
                    QSet<QString> providers;
                    for (const QString &capability : nonEmptyLines(requiresOut)) {
                        const QString provider = packageNameFromDependencyCapability(capability);
                        if (!provider.isEmpty() && provider != rpmName)
                            providers.insert(provider);
                    }

                    pending->remaining += providers.size();
                    for (const QString &provider : providers) {
                        runCommand("rpm", {"-q", provider}, [this, pending, provider, finish](int queryExit, const QString &, const QString &) {
                            if (queryExit != 0) {
                                finish();
                                return;
                            }

                            pending->remaining += 1;
                            runCommand("rpm", {"-qc", provider}, [pending, provider, finish](int configExit, const QString &configOut, const QString &) {
                                if (configExit == 0) {
                                    for (const QString &path : nonEmptyLines(configOut)) {
                                        if (pending->seenPaths.contains(path))
                                            continue;
                                        pending->seenPaths.insert(path);
                                        pending->rows << QStringList{
                                            "Related Config",
                                            path,
                                            "RPM config",
                                            "Owned by direct dependency " + provider
                                        };
                                    }
                                }
                                finish();
                            });
                            finish();
                        });
                    }
                } else if (!requiresErr.trimmed().isEmpty()) {
                    emit errorOccurred(requiresErr.trimmed());
                }
                finish();
            });
        } else {
            const QString message = err.trimmed().isEmpty()
                ? QString("rpm -ql %1 failed with exit code %2").arg(rpmName).arg(exitCode)
                : err.trimmed();
            emit errorOccurred(message);
            emit userConfigLoaded(name, {
                QStringList{"Error", message, "RPM query failed", "The package is not installed or RPM could not list its files."}
            });
        }
    });
}

void Dnf5CliBackend::loadRelatedConfig(const QString &name) {
    struct PendingRelatedConfig {
        int remaining = 0;
        QList<QStringList> rows;
        QSet<QString> seenPaths;
    };

    auto pending = QSharedPointer<PendingRelatedConfig>::create();
    pending->remaining = 1;

    auto finish = [this, pending, name]() {
        --pending->remaining;
        if (pending->remaining != 0)
            return;

        if (pending->rows.isEmpty()) {
            pending->rows << QStringList{
                "No related config found",
                "—",
                "No RPM config files were found on simple direct dependency packages.",
                "Checked package-looking direct dependencies"
            };
        }
        emit relatedConfigLoaded(name, pending->rows);
    };

    runCommand("dnf5", {"repoquery", "--requires", name}, [this, pending, name, finish](int exitCode, const QString &out, const QString &err) {
        if (exitCode == 0) {
            QSet<QString> providers;
            for (const QString &capability : nonEmptyLines(out)) {
                const QString provider = packageNameFromDependencyCapability(capability);
                if (!provider.isEmpty() && provider != name)
                    providers.insert(provider);
            }

            pending->remaining += providers.size();
            for (const QString &provider : providers) {
                runCommand("rpm", {"-q", provider}, [this, pending, provider, finish](int queryExit, const QString &, const QString &) {
                    if (queryExit != 0) {
                        finish();
                        return;
                    }

                    pending->remaining += 1;
                    runCommand("rpm", {"-qc", provider}, [this, pending, provider, finish](int configExit, const QString &configOut, const QString &) {
                        if (configExit == 0) {
                            for (const QString &path : nonEmptyLines(configOut)) {
                                if (pending->seenPaths.contains(path))
                                    continue;
                                pending->seenPaths.insert(path);
                                pending->rows << QStringList{
                                    provider,
                                    path,
                                    "RPM config",
                                    "Owned by direct dependency " + provider
                                };
                            }
                        }
                        finish();
                    });
                    finish();
                });
            }
        } else if (!err.trimmed().isEmpty()) {
            emit errorOccurred(err.trimmed());
        }
        finish();
    });
}

void Dnf5CliBackend::runCommand(const QString &program, const QStringList &arguments,
                                std::function<void(int, const QString &, const QString &)> handler) {
    auto *process = new Dnf5Process(this);
    connect(process, &Dnf5Process::started, this, &PackageBackend::commandStarted);
    connect(process, &Dnf5Process::failed, this, [this, process](const QString &command, const QString &message) {
        emit commandFinished(command + ": " + message, false);
        emit errorOccurred(message);
        process->deleteLater();
    });
    connect(process, &Dnf5Process::finished, this,
            [this, process, handler](const QString &command, int exitCode, const QString &out, const QString &err) {
        emit commandFinished(command, exitCode == 0);
        handler(exitCode, out, err);
        process->deleteLater();
    });
    process->run(program, arguments);
}

Package Dnf5CliBackend::packageFromRepoqueryLine(const QString &line) {
    Package package;
    package.id.name = line.trimmed();
    package.summary = "Package metadata not loaded yet";
    return package;
}

Package Dnf5CliBackend::packageFromDelimitedLine(const QString &line) {
    const QStringList parts = line.split('|');
    Package package;
    if (parts.size() < 16)
        return package;

    auto value = [&parts](int index) {
        return index < parts.size() ? parts[index].trimmed() : QString();
    };

    package.id.name = value(0);
    package.id.epoch = value(1);
    package.id.version = value(2);
    package.id.release = value(3);
    package.id.arch = value(4);
    const QString repoid = value(5);
    const QString fromRepo = value(6);

    // repoid is the actual install-source repo in DNF5 (may be "@System" on older versions).
    // from_repo is whichever enabled repo currently provides this in metadata — it is NOT
    // the original install source. Prefer repoid when it's meaningful.
    if (!repoid.isEmpty() && repoid != "@System") {
        package.repoId = repoid;
    } else {
        package.repoId = fromRepo;
    }

    package.installReason = value(7);
    package.installSize = value(8).toLongLong();
    const qint64 installEpoch = value(9).toLongLong();
    if (installEpoch > 0)
        package.installTime = QDateTime::fromSecsSinceEpoch(installEpoch);
    package.summary = value(10);
    package.license = value(11);
    package.url = value(12);
    package.sourceRpm = value(13);
    package.vendor = value(14);
    package.packager = value(15);
    // repoid==@System → classic DNF4-style installed indicator.
    // from_repo non-empty → package was installed from somewhere (regardless of repoid).
    // Both false → likely an --available query result (repoid is the actual repo, from_repo empty).
    package.installed = (repoid == "@System") || !fromRepo.isEmpty();
    return package;
}

Package Dnf5CliBackend::packageFromInfoOutput(const QString &text, const QString &fallbackName) {
    Package package;
    package.id.name = fallbackName;
    package.installed = true;

    const QStringList lines = text.split('\n');
    bool inDescription = false;
    for (const QString &line : lines) {
        const int colon = line.indexOf(':');
        if (colon == -1) {
            if (inDescription && !line.trimmed().isEmpty())
                package.description += "\n" + line.trimmed();
            continue;
        }
        const QString key = line.left(colon).trimmed().toLower();
        const QString value = line.mid(colon + 1).trimmed();
        inDescription = false;

        if (key == "name") package.id.name = value;
        else if (key == "epoch") package.id.epoch = value;
        else if (key == "version") package.id.version = value;
        else if (key == "release") package.id.release = value;
        else if (key == "architecture" || key == "arch") package.id.arch = value;
        else if (key == "size") package.summary = package.summary.isEmpty() ? QString() : package.summary;
        else if (key == "source") package.sourceRpm = value;
        else if (key == "repository" || key == "repo") package.repoId = value;
        else if (key == "summary") package.summary = value;
        else if (key == "url") package.url = value;
        else if (key == "license") package.license = value;
        else if (key == "description") {
            package.description = value;
            inDescription = true;
        }
    }

    if (package.summary.isEmpty())
        package.summary = "No summary available";
    return package;
}

QList<Package> Dnf5CliBackend::packagesFromDelimitedOutput(const QString &text) {
    QList<Package> packages;
    for (const QString &line : nonEmptyLines(text)) {
        Package package = packageFromDelimitedLine(line);
        if (package.id.isValid())
            packages << package;
    }
    return packages;
}

QList<DependencyEdge> Dnf5CliBackend::dependencyEdgesFromLines(const QString &text, DependencyType type, const QString &packageName) {
    QList<DependencyEdge> edges;
    for (const QString &line : nonEmptyLines(text)) {
        DependencyEdge edge;
        edge.from.name = packageName;
        edge.type = type;
        edge.capability = line.trimmed();
        edges << edge;
    }
    return edges;
}

QStringList Dnf5CliBackend::nonEmptyLines(const QString &text) {
    QStringList lines;
    for (const QString &line : text.split('\n')) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            lines << trimmed;
    }
    return lines;
}

QList<QStringList> Dnf5CliBackend::historyRowsFromOutput(const QString &text, const QString &packageName) {
    QList<QStringList> rows;
    const QStringList lines = nonEmptyLines(text);
    for (const QString &line : lines) {
        if (line.startsWith("ID ") || line.startsWith("No transaction", Qt::CaseInsensitive))
            continue;

        static const QRegularExpression re(R"(^\s*(\d+)\s+(.+?)\s+(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2})\s+(.*?)\s+(\d+)\s*$)");
        const auto match = re.match(line);
        if (match.hasMatch()) {
            rows << QStringList{
                match.captured(1),
                match.captured(3),
                match.captured(4).trimmed().isEmpty() ? "Changed" : match.captured(4).trimmed(),
                packageName,
                match.captured(2).trimmed()
            };
        } else {
            rows << QStringList{"—", "—", "Transaction", packageName, line};
        }
    }

    if (rows.isEmpty())
        rows << QStringList{"—", "—", "No matching history", packageName, "DNF history did not report a transaction for this package."};
    return rows;
}

QString Dnf5CliBackend::packageQueryFormat() {
    return "%{name}|%{epoch}|%{version}|%{release}|%{arch}|%{repoid}|%{from_repo}|%{reason}|%{installsize}|%{installtime}|%{summary}|%{license}|%{url}|%{sourcerpm}|%{vendor}|%{packager}\\n";
}

QString Dnf5CliBackend::packageNameFromNevra(const QString &text) {
    const QString trimmed = text.trimmed();
    const int epoch = trimmed.indexOf(QRegularExpression(R"(-\d+:)"));
    if (epoch > 0)
        return trimmed.left(epoch);
    const int version = trimmed.indexOf(QRegularExpression(R"(-\d)"));
    return version > 0 ? trimmed.left(version) : trimmed;
}

QString Dnf5CliBackend::repoKind(const QString &repoId) {
    const QString lower = repoId.toLower();
    if (lower == "fedora" || lower == "updates" || lower.startsWith("fedora-") || lower.startsWith("updates-"))
        return "Fedora repository";
    if (lower.contains("copr"))
        return "COPR repository";
    if (lower.contains("rpmfusion"))
        return "RPM Fusion repository";
    if (lower == "@system")
        return "Installed system package";
    if (lower.isEmpty())
        return "Unknown";
    return "Third-party repository";
}

QList<QStringList> Dnf5CliBackend::userConfigRows(const QString &packageName, const QStringList &ownedFiles) {
    QList<QStringList> rows;
    QSet<QString> seenPaths;

    rows << QStringList{
        "Note",
        "System config comes from RPM-owned /etc paths. User config is guessed because RPM does not track files in your home folder.",
        "Read-only",
        "RPM file list plus XDG config/data/cache guesses"
    };

    for (const QString &path : ownedFiles) {
        if (!path.startsWith("/etc"))
            continue;
        if (seenPaths.contains(path))
            continue;
        seenPaths.insert(path);

        QFileInfo info(path);
        rows << QStringList{
            "System Config",
            path,
            info.isDir() ? "Directory" : "File",
            "Owned by package " + packageName
        };
    }

    const QString configHome = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString cacheHome = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    const QList<QPair<QString, QString>> roots = {
        {"Config", configHome},
        {"Data", dataHome},
        {"Cache", cacheHome},
    };

    const QStringList names = candidateAppNames(packageName, ownedFiles);
    QSet<QString> seen;

    for (const auto &root : roots) {
        if (root.second.isEmpty())
            continue;
        for (const QString &name : names) {
            const QString path = QDir(root.second).filePath(name);
            if (seen.contains(path))
                continue;
            seen.insert(path);

            QFileInfo info(path);
            if (!info.exists())
                continue;
            if (seenPaths.contains(path))
                continue;
            seenPaths.insert(path);

            rows << QStringList{
                "User " + root.first,
                path,
                info.isDir() ? "Directory" : "File",
                QString("Matched likely app name '%1'").arg(name)
            };
        }
    }

    if (rows.size() == 1) {
        rows << QStringList{
            "No match",
            "No package-owned /etc config or likely user config/data/cache path was found.",
            "None found",
            "Checked RPM-owned /etc paths, package name, owned binaries, and desktop file IDs"
        };
    }

    return rows;
}

QStringList Dnf5CliBackend::candidateAppNames(const QString &packageName, const QStringList &ownedFiles) {
    QSet<QString> names;
    auto addName = [&names](const QString &raw) {
        QString name = raw.trimmed();
        if (name.isEmpty())
            return;
        if (name.endsWith(".desktop"))
            name.chop(QString(".desktop").size());
        if (name.endsWith(".appdata.xml"))
            name.chop(QString(".appdata.xml").size());
        if (name.endsWith(".metainfo.xml"))
            name.chop(QString(".metainfo.xml").size());
        names.insert(name);
    };

    addName(packageName);
    if (packageName.contains('-')) {
        const QStringList parts = packageName.split('-', Qt::SkipEmptyParts);
        if (!parts.isEmpty())
            addName(parts.first());
        if (packageName.endsWith("-shell"))
            addName(packageName.left(packageName.size() - QString("-shell").size()));
        if (packageName.endsWith("-gui"))
            addName(packageName.left(packageName.size() - QString("-gui").size()));
        if (packageName.endsWith("-app"))
            addName(packageName.left(packageName.size() - QString("-app").size()));
    }

    for (const QString &file : ownedFiles) {
        QFileInfo info(file);
        if ((file.startsWith("/usr/bin/") || file.startsWith("/usr/sbin/")) && info.isFile())
            addName(info.fileName());
        if (file.endsWith(".desktop")) {
            addName(info.fileName());
            const QString base = info.completeBaseName();
            if (base.contains('.'))
                addName(base.section('.', -1));
            if (base.contains('-'))
                addName(base.section('-', 0, 0));
        }
        if (file.endsWith(".appdata.xml") || file.endsWith(".metainfo.xml")) {
            addName(info.fileName());
            QString base = info.fileName();
            base.remove(".appdata.xml");
            base.remove(".metainfo.xml");
            if (base.contains('.'))
                addName(base.section('.', -1));
            if (base.contains('-'))
                addName(base.section('-', 0, 0));
        }
    }

    QStringList sorted = names.values();
    sorted.sort(Qt::CaseInsensitive);
    return sorted;
}

QString Dnf5CliBackend::packageNameFromDependencyCapability(const QString &capability) {
    QString text = capability.trimmed();
    const int space = text.indexOf(' ');
    if (space > 0)
        text = text.left(space);
    if (text.startsWith('/'))
        return QString();
    if (text.contains(".so"))
        return QString();
    if (text.contains("rtld", Qt::CaseInsensitive))
        return QString();
    if (text.contains("GNU_HASH"))
        return QString();
    if (text.contains("GLIBC"))
        return QString();
    if (text.contains("CXXABI"))
        return QString();
    if (text.contains("GLIBCXX"))
        return QString();
    const int paren = text.indexOf('(');
    if (paren > 0)
        text = text.left(paren);
    if (text.contains(')'))
        return QString();
    static const QRegularExpression plausible(R"(^[A-Za-z0-9_+.-]+$)");
    return plausible.match(text).hasMatch() ? text : QString();
}
