#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("NexImageEditor");
    QApplication::setOrganizationName("Hyggshi");
    QApplication::setApplicationVersion("2.0.0");
    Theme::apply(app);

    MainWindow win;
    win.show();
    for (int i = 1; i < argc; ++i) win.openPath(QString::fromLocal8Bit(argv[i]));
    return app.exec();
}
