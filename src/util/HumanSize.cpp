#include "HumanSize.h"

#include <QLocale>

QString humanSize(qint64 bytes) {
    const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }

    const int precision = unit == 0 ? 0 : 1;
    return QLocale().toString(value, 'f', precision) + " " + units[unit];
}
