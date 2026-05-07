#pragma once

#include <QDateTime>
#include <QString>

struct PackageId {
    QString name;
    QString epoch;
    QString version;
    QString release;
    QString arch;

    QString nevra() const;
    QString evr() const;
    bool isValid() const;
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
    QString installReason;
    bool installed = false;
    qint64 installSize = 0;
    QDateTime installTime;
};
