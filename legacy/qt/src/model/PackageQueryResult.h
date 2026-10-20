#pragma once

#include <QList>
#include <QString>

#include "Package.h"

struct PackageQueryResult {
    enum class MatchKind {
        ExactPackage,
        FileOwner,
        CapabilityProvider,
        AvailablePackage,
        FuzzyPackage,
        Unknown
    };

    Package package;
    MatchKind matchKind = MatchKind::Unknown;
    QString matchedText;
};

QString matchKindLabel(PackageQueryResult::MatchKind kind);
