#pragma once

#include <functional>

#include "PackageBackend.h"

class Dnf5CliBackend : public PackageBackend {
    Q_OBJECT

public:
    explicit Dnf5CliBackend(QObject *parent = nullptr);

    void checkTools() override;
    void search(const QString &query) override;
    void loadPackage(const QString &name) override;
    void loadDependencies(const QString &name) override;
    void loadRequiredBy(const QString &name) override;
    void loadFiles(const QString &name) override;
    void loadHistory(const QString &name) override;
    void loadRepositoryInfo(const QString &name) override;
    void loadImpact(const QString &name) override;
    void loadUserConfig(const QString &name) override;
    void loadRelatedConfig(const QString &name) override;

private:
    void runCommand(const QString &program, const QStringList &arguments,
                    std::function<void(int, const QString &, const QString &)> handler);
    static Package packageFromRepoqueryLine(const QString &line);
    static Package packageFromDelimitedLine(const QString &line);
    static Package packageFromInfoOutput(const QString &text, const QString &fallbackName);
    static QList<Package> packagesFromDelimitedOutput(const QString &text);
    static QList<DependencyEdge> dependencyEdgesFromLines(const QString &text, DependencyType type, const QString &packageName);
    static QList<QStringList> historyRowsFromOutput(const QString &text, const QString &packageName);
    static QStringList nonEmptyLines(const QString &text);
    static QString packageQueryFormat();
    static QString packageNameFromNevra(const QString &text);
    static QString repoKind(const QString &repoId);
    static QList<QStringList> userConfigRows(const QString &packageName, const QStringList &ownedFiles);
    static QStringList candidateAppNames(const QString &packageName, const QStringList &ownedFiles);
    static QString packageNameFromDependencyCapability(const QString &capability);
};
