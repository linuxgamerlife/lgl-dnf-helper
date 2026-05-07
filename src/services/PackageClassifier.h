#pragma once

#include <QStringList>

#include "model/Package.h"

class PackageClassifier {
public:
    QStringList badgesForPackage(const Package &package) const;
    bool isThirdPartyRepo(const QString &repoId) const;
    bool isSystemSensitive(const QString &name) const;
};
