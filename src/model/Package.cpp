#include "Package.h"

QString PackageId::evr() const {
    const QString versionRelease = release.isEmpty() ? version : version + "-" + release;
    return epoch.isEmpty() || epoch == "0" ? versionRelease : epoch + ":" + versionRelease;
}

QString PackageId::nevra() const {
    QString text = name;
    const QString evrText = evr();
    if (!evrText.isEmpty())
        text += "-" + evrText;
    if (!arch.isEmpty())
        text += "." + arch;
    return text;
}

bool PackageId::isValid() const {
    return !name.trimmed().isEmpty();
}
