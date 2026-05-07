#include "PackageQueryResult.h"

QString matchKindLabel(PackageQueryResult::MatchKind kind) {
    switch (kind) {
        case PackageQueryResult::MatchKind::ExactPackage: return "Exact package";
        case PackageQueryResult::MatchKind::FileOwner: return "Owns file";
        case PackageQueryResult::MatchKind::CapabilityProvider: return "Provides capability";
        case PackageQueryResult::MatchKind::AvailablePackage: return "Available package";
        case PackageQueryResult::MatchKind::FuzzyPackage: return "Package match";
        case PackageQueryResult::MatchKind::Unknown: return "Unknown";
    }
    return "Unknown";
}
