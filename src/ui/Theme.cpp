#include "ui/Theme.h"

#include <QApplication>
#include <QPalette>

namespace Theme {

QColor canvasBackground() { return QColor(0x3c, 0x3c, 0x3c); }
QColor panelBackground() { return QColor(0x31, 0x31, 0x31); }

void apply(QApplication& app) {
    app.setStyle("Fusion");
    QPalette pal;
    const QColor window(0x2b, 0x2b, 0x2b), base(0x1f, 0x1f, 0x1f), text(0xd6, 0xd6, 0xd6);
    pal.setColor(QPalette::Window, window);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Base, base);
    pal.setColor(QPalette::AlternateBase, window);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Button, QColor(0x3a, 0x3a, 0x3a));
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::ToolTipBase, QColor(0x20, 0x20, 0x20));
    pal.setColor(QPalette::ToolTipText, text);
    pal.setColor(QPalette::Highlight, QColor(0x31, 0x7f, 0xc6));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(0x77, 0x77, 0x77));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x77, 0x77, 0x77));
    app.setPalette(pal);
    app.setStyleSheet(
        "QMenuBar { background:#2b2b2b; }"
        "QMenuBar::item:selected, QMenu::item:selected { background:#3d6a99; }"
        "QMenu { background:#2b2b2b; border:1px solid #111; }"
        "QToolBar { background:#2b2b2b; border:none; spacing:2px; }"
        "QToolButton { border:1px solid transparent; border-radius:3px; padding:3px; }"
        "QToolButton:checked, QToolButton:hover { background:#4a4a4a; border:1px solid #1a1a1a; }"
        "QDockWidget::title { background:#323232; padding:4px; }"
        "QTabBar::tab { background:#333; padding:4px 10px; border:1px solid #1a1a1a; }"
        "QTabBar::tab:selected { background:#4a4a4a; }"
        "QStatusBar { background:#2b2b2b; }"
        "QListWidget { background:#262626; border:1px solid #1a1a1a; }"
        "QListWidget::item:selected { background:#3d6a99; }");
}

}  // namespace Theme
