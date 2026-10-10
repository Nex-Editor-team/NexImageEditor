#include "CropGeometry.h"
#include <algorithm>
#include <cmath>

QRect CropGeometry::fromDrag(QPoint anchor, QPoint end, QSize bounds, QSize aspect) {
    if (bounds.isEmpty()) return {};
    anchor.setX(std::clamp(anchor.x(), 0, bounds.width()));
    anchor.setY(std::clamp(anchor.y(), 0, bounds.height()));
    end.setX(std::clamp(end.x(), 0, bounds.width()));
    end.setY(std::clamp(end.y(), 0, bounds.height()));
    const int dx = end.x() - anchor.x(), dy = end.y() - anchor.y();
    int w = std::abs(dx), h = std::abs(dy);
    if (!aspect.isEmpty()) {
        const int units = std::min(w / aspect.width(), h / aspect.height());
        w = units * aspect.width();
        h = units * aspect.height();
    }
    if (w == 0 || h == 0) return {};
    return QRect(QPoint(dx < 0 ? anchor.x() - w : anchor.x(),
                        dy < 0 ? anchor.y() - h : anchor.y()), QSize(w, h));
}
