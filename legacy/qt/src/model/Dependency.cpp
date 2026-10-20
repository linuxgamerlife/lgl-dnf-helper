#include "Dependency.h"

QString dependencyTypeLabel(DependencyType type) {
    switch (type) {
        case DependencyType::Requires: return "Requires";
        case DependencyType::RequiresPre: return "Requires-pre";
        case DependencyType::Recommends: return "Recommends";
        case DependencyType::Suggests: return "Suggests";
        case DependencyType::Supplements: return "Supplements";
        case DependencyType::Enhances: return "Enhances";
        case DependencyType::Provides: return "Provides";
        case DependencyType::Conflicts: return "Conflicts";
        case DependencyType::Obsoletes: return "Obsoletes";
        case DependencyType::Unknown: return "Unknown";
    }
    return "Unknown";
}
