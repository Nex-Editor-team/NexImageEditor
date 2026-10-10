#include "ui/Icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <functional>

namespace {

QIcon makeIcon(const std::function<void(QPainter&)>& draw) {
    QPixmap pm(24, 24);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(0xdd, 0xdd, 0xdd), 1.6);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    draw(p);
    p.end();
    return QIcon(pm);
}

}  // namespace

namespace Icons {

QIcon tool(ToolId id) {
    return makeIcon([id](QPainter& p) {
        switch (id) {
        case ToolId::Move: {
            p.drawLine(12, 3, 12, 21);
            p.drawLine(3, 12, 21, 12);
            p.drawLine(12, 3, 9, 6);
            p.drawLine(12, 3, 15, 6);
            p.drawLine(12, 21, 9, 18);
            p.drawLine(12, 21, 15, 18);
            break;
        }
        case ToolId::RectMarquee: p.setPen(QPen(QColor(0xdd, 0xdd, 0xdd), 1.2, Qt::DashLine)); p.drawRect(QRectF(4, 5, 16, 14)); break;
        case ToolId::EllipseMarquee: p.setPen(QPen(QColor(0xdd, 0xdd, 0xdd), 1.2, Qt::DashLine)); p.drawEllipse(QRectF(4, 5, 16, 14)); break;
        case ToolId::Lasso: {
            QPainterPath path;
            path.moveTo(4, 14);
            path.cubicTo(2, 4, 20, 2, 19, 10);
            path.cubicTo(18, 18, 8, 21, 6, 17);
            p.drawPath(path);
            break;
        }
        case ToolId::MagicWand:
            p.drawLine(5, 19, 15, 9);
            p.drawLine(16, 3, 16, 7);
            p.drawLine(14, 5, 18, 5);
            p.drawLine(20, 9, 20, 11);
            break;
        case ToolId::Crop: p.drawLine(6, 2, 6, 18); p.drawLine(6, 18, 22, 18); p.drawLine(18, 22, 18, 6); p.drawLine(2, 6, 18, 6); break;
        case ToolId::Eyedropper: p.drawLine(16, 4, 20, 8); p.drawLine(14, 6, 18, 10); p.drawLine(4, 16, 14, 6); p.drawLine(3, 21, 6, 18); break;
        case ToolId::Brush: {
            QPainterPath path;
            path.moveTo(20, 4);
            path.quadTo(12, 8, 9, 14);
            path.lineTo(7, 19);
            path.lineTo(10, 17);
            path.quadTo(14, 14, 20, 4);
            p.drawPath(path);
            break;
        }
        case ToolId::Eraser: p.drawRoundedRect(QRectF(4, 8, 16, 10), 2, 2); p.drawLine(9, 8, 15, 18); break;
        case ToolId::Bucket: {
            QPainterPath path;
            path.moveTo(5, 9); path.lineTo(11, 3); path.lineTo(19, 11); path.lineTo(13, 17); path.closeSubpath();
            p.drawPath(path);
            p.setBrush(QColor(0xdd, 0xdd, 0xdd));
            p.drawEllipse(QPointF(20, 16), 2, 3);
            break;
        }
        case ToolId::Gradient: p.drawRect(QRectF(3, 5, 18, 14)); p.drawLine(3, 12, 21, 12); break;
        case ToolId::Text: p.drawLine(5, 5, 19, 5); p.drawLine(12, 5, 12, 20); p.drawLine(8, 20, 16, 20); break;
        case ToolId::ShapeRect: p.drawRect(QRectF(4, 6, 16, 12)); break;
        case ToolId::ShapeEllipse: p.drawEllipse(QRectF(4, 6, 16, 12)); break;
        case ToolId::Transform: p.drawRect(QRectF(5, 5, 14, 14)); p.setBrush(QColor(0xdd, 0xdd, 0xdd)); p.drawRect(QRectF(3, 3, 4, 4)); p.drawRect(QRectF(17, 17, 4, 4)); break;
        case ToolId::Zoom: p.drawEllipse(QPointF(10, 10), 6, 6); p.drawLine(15, 15, 21, 21); p.drawLine(7, 10, 13, 10); p.drawLine(10, 7, 10, 13); break;
        case ToolId::Hand: {
            QPainterPath path;
            path.moveTo(6, 20); path.lineTo(4, 12); path.lineTo(8, 14); path.lineTo(8, 4);
            path.lineTo(10, 4); path.lineTo(10, 12); path.lineTo(12, 2); path.lineTo(14, 4);
            path.lineTo(14, 12); path.lineTo(18, 6); path.lineTo(20, 9); path.lineTo(16, 20); path.closeSubpath();
            p.drawPath(path);
            break;
        }
        }
    });
}

QIcon visibility(bool visible) {
    return makeIcon([visible](QPainter& p) {
        if (visible) {
            p.drawEllipse(QRectF(3, 7, 18, 10));
            p.setBrush(QColor(0xdd, 0xdd, 0xdd));
            p.drawEllipse(QPointF(12, 12), 2.5, 2.5);
        } else {
            p.drawLine(4, 4, 20, 20);
            p.drawEllipse(QRectF(3, 7, 18, 10));
        }
    });
}

QIcon lock(bool locked) {
    return makeIcon([locked](QPainter& p) {
        p.drawRoundedRect(QRectF(5, 11, 14, 10), 1, 1);
        if (locked) p.drawArc(QRectF(8, 4, 8, 12), 0, 180 * 16);
        else p.drawArc(QRectF(8, 2, 8, 12), 0, 180 * 16);
    });
}

}  // namespace Icons
