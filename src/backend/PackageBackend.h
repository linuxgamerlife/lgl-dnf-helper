#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "model/Dependency.h"
#include "model/Package.h"
#include "model/PackageQueryResult.h"

class PackageBackend : public QObject {
    Q_OBJECT

public:
    explicit PackageBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~PackageBackend() override = default;

    virtual void checkTools() = 0;
    virtual void search(const QString &query) = 0;
    virtual void loadPackage(const QString &name) = 0;
    virtual void loadDependencies(const QString &name) = 0;
    virtual void loadRequiredBy(const QString &name) = 0;
    virtual void loadFiles(const QString &name) = 0;
    virtual void loadHistory(const QString &name) = 0;
    virtual void loadRepositoryInfo(const QString &name) = 0;
    virtual void loadImpact(const QString &name) = 0;
    virtual void loadUserConfig(const QString &name) = 0;
    virtual void loadRelatedConfig(const QString &name) = 0;

signals:
    void toolsChecked(bool dnf5Available, bool rpmAvailable);
    void searchCompleted(const QList<PackageQueryResult> &results);
    void packageLoaded(const QString &requestName, const Package &package);
    void dependenciesLoaded(const QString &requestName, const QList<DependencyEdge> &dependencies);
    void requiredByLoaded(const QString &requestName, const QList<DependencyEdge> &dependencies);
    void filesLoaded(const QString &requestName, const QStringList &files);
    void historyLoaded(const QString &requestName, const QList<QStringList> &rows);
    void repositoryInfoLoaded(const QString &requestName, const QList<QStringList> &rows);
    void impactLoaded(const QString &requestName, const QList<QStringList> &rows);
    void userConfigLoaded(const QString &requestName, const QList<QStringList> &rows);
    void relatedConfigLoaded(const QString &requestName, const QList<QStringList> &rows);
    void commandStarted(const QString &command);
    void commandFinished(const QString &summary, bool ok);
    void errorOccurred(const QString &message);
};
