#pragma once

#include <QList>
#include <QString>

#include "Package.h"

enum class DependencyType {
    Requires,
    RequiresPre,
    Recommends,
    Suggests,
    Supplements,
    Enhances,
    Provides,
    Conflicts,
    Obsoletes,
    Unknown
};

QString dependencyTypeLabel(DependencyType type);

struct DependencyEdge {
    PackageId from;
    QString capability;
    DependencyType type = DependencyType::Unknown;
    PackageId installedProvider;
    QList<PackageId> availableProviders;
};
