#include <QApplication>
#include <QStyleFactory>
#include "gui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("NexImageEditor");
    QApplication::setOrganizationName("Nex");
    if (QStyleFactory::keys().contains("Fusion"))
        QApplication::setStyle(QStyleFactory::create("Fusion"));
    app.setStyleSheet(R"(
        QWidget { background: #27282a; color: #d9dadd; font-family: "Segoe UI", sans-serif; font-size: 12px; }
        QMainWindow, QDialog { background: #242527; }
        QMenuBar { background: #2b2c2e; color: #bfc0c4; padding: 3px; }
        QMenuBar::item { padding: 5px 9px; background: transparent; }
        QMenuBar::item:selected, QMenu::item:selected { background: #414954; color: white; }
        QMenu { background: #303134; border: 1px solid #4a4b4e; padding: 5px; }
        QMenu::item { padding: 6px 28px 6px 12px; }
        QMenu::separator { height: 1px; background: #494a4d; margin: 4px; }
        QToolBar { background: #242527; border: none; border-bottom: 1px solid #3d3e41; spacing: 4px; padding: 4px; }
        QToolBar#toolsBar { border-right: 1px solid #3d3e41; padding: 6px; spacing: 5px; }
        QToolBar#toolsBar QToolButton { min-width: 32px; min-height: 32px; font-size: 19px; }
        QToolButton { background: transparent; border: 1px solid transparent; border-radius: 3px; padding: 5px; }
        QToolButton:hover, QPushButton:hover { background: #3d4045; }
        QToolButton:checked { background: #3b4e66; border: 1px solid #698ebf; }
        QPushButton { background: #333538; border: 1px solid #484a4d; border-radius: 3px; padding: 6px 9px; }
        QTabWidget::pane { border: 0; border-top: 1px solid #414245; }
        QTabBar::tab { background: #27282a; color: #96999e; padding: 10px 14px; }
        QTabBar::tab:selected { color: #f1f1f2; border-bottom: 2px solid #92b7ed; }
        QLabel#documentTab { background: #303133; border-bottom: 1px solid #414245; color: #e1e2e4; }
        QListWidget { background: #27282a; border: none; outline: none; }
        QListWidget::item { border-bottom: 1px solid #36383b; padding: 5px; }
        QListWidget::item:selected { background: #41464e; color: #fff; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { background: #202123; border: 1px solid #47494c; border-radius: 3px; padding: 5px; selection-background-color: #5476a1; }
        QGroupBox { border: 1px solid #414347; border-radius: 3px; margin-top: 12px; padding-top: 10px; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QSplitter::handle { background: #3b3c3f; }
        QStatusBar { background: #292a2c; border-top: 1px solid #3d3e40; color: #a6a8ad; }
        QScrollArea { border: 0; }
        QScrollBar:vertical { background: #27282a; width: 8px; }
        QScrollBar::handle:vertical { background: #55575b; min-height: 20px; border-radius: 4px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )");
    MainWindow window;
    window.resize(1400, 900);
    window.show();
    return app.exec();
}
