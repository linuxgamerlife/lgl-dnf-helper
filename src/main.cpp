#include <QApplication>
#include <QIcon>

#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("LGL DNF Helper");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("lgl-dnf-helper");
    app.setWindowIcon(QIcon(":/icons/lgl-dnf-helper_icon.svg"));

    MainWindow window;
    window.show();
    return app.exec();
}
