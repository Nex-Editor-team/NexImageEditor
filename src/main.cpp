#include <QApplication>
#include <QStyleFactory>
#include "gui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("NexImageEditor");
    QApplication::setOrganizationName("Nex");

    if (QStyleFactory::keys().contains("Fusion")) {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
    }

    MainWindow window;
    window.resize(1400, 900);
    window.show();

    return app.exec();
}
