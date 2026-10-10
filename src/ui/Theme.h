#pragma once
#include <QColor>

class QApplication;

namespace Theme {
// Photoshop-like dark theme applied through the application palette and stylesheet.
void apply(QApplication& app);
QColor canvasBackground();
QColor panelBackground();
}  // namespace Theme
