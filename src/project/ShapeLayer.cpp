#include "ShapeLayer.h"
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QtMath>
#include <algorithm>

ShapeLayer::ShapeLayer(ShapeKind kind, QString name)
    : Layer(LayerType::Shape, std::move(name)), kind(kind) {
    regenerate();
}

namespace {
// Builds a regular N-gon path centered in `r`, point-up, optionally
// alternating outer/inner radius (used for stars).
QPainterPath regularPolygon(const QRectF& r, int sides, double startAngleDeg = -90.0,
                            double innerRatio = 1.0) {
    QPainterPath path;
    QPointF center = r.center();
    double rx = r.width() / 2.0;
    double ry = r.height() / 2.0;
    int pointCount = innerRatio < 1.0 ? sides * 2 : sides;

    for (int i = 0; i < pointCount; ++i) {
        double angle = qDegreesToRadians(startAngleDeg + i * (360.0 / pointCount));
        double radiusScale = (innerRatio < 1.0 && i % 2 == 1) ? innerRatio : 1.0;
        QPointF p(center.x() + rx * radiusScale * std::cos(angle),
                  center.y() + ry * radiusScale * std::sin(angle));
        if (i == 0) path.moveTo(p); else path.lineTo(p);
    }
    path.closeSubpath();
    return path;
}
} // namespace

QPainterPath ShapeLayer::buildPath(const QRectF& r) const {
    QPainterPath path;
    switch (kind) {
        case ShapeKind::Rectangle:
            path.addRect(r);
            break;
        case ShapeKind::RoundedRectangle:
            path.addRoundedRect(r, cornerRadius, cornerRadius);
            break;
        case ShapeKind::Ellipse:
            path.addEllipse(r);
            break;
        case ShapeKind::Triangle:
            path.moveTo(r.center().x(), r.top());
            path.lineTo(r.bottomLeft());
            path.lineTo(r.bottomRight());
            path.closeSubpath();
            break;
        case ShapeKind::Pentagon:
            path = regularPolygon(r, 5);
            break;
        case ShapeKind::Hexagon:
            path = regularPolygon(r, 6, -90.0);
            break;
        case ShapeKind::Star:
            path = regularPolygon(r, 5, -90.0, 0.45);
            break;
        case ShapeKind::Arrow: {
            // Rightward arrow, proportioned relative to r.
            double w = r.width(), h = r.height();
            double shaftTop = r.top() + h * 0.30;
            double shaftBottom = r.top() + h * 0.70;
            double headStart = r.left() + w * 0.60;
            path.moveTo(r.left(), shaftTop);
            path.lineTo(headStart, shaftTop);
            path.lineTo(headStart, r.top());
            path.lineTo(r.right(), r.top() + h * 0.5);
            path.lineTo(headStart, r.bottom());
            path.lineTo(headStart, shaftBottom);
            path.lineTo(r.left(), shaftBottom);
            path.closeSubpath();
            break;
        }
        case ShapeKind::Line:
            // Line has no fill area; handled separately in regenerate().
            break;
    }
    return path;
}

QBrush ShapeLayer::buildBrush(const QRectF& r) const {
    switch (fillMode) {
        case ShapeFillMode::Solid:
            return QBrush(fillColor);

        case ShapeFillMode::LinearGradient: {
            double rad = qDegreesToRadians(gradientAngleDeg);
            QPointF center = r.center();
            double dx = std::cos(rad) * r.width() / 2.0;
            double dy = std::sin(rad) * r.height() / 2.0;
            QLinearGradient grad(center - QPointF(dx, dy), center + QPointF(dx, dy));
            grad.setColorAt(0.0, fillColor);
            grad.setColorAt(1.0, fillColor2);
            return QBrush(grad);
        }
        case ShapeFillMode::RadialGradient: {
            QRadialGradient grad(r.center(), std::max(r.width(), r.height()) / 2.0);
            grad.setColorAt(0.0, fillColor);
            grad.setColorAt(1.0, fillColor2);
            return QBrush(grad);
        }
    }
    return QBrush(fillColor);
}

void ShapeLayer::regenerate() {
    int pad = strokeWidth;
    QImage img(localSize.width() + pad * 2, localSize.height() + pad * 2, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QRectF r(pad, pad, localSize.width(), localSize.height());

    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (kind == ShapeKind::Line) {
        QPen pen(strokeColor.isValid() ? strokeColor : fillColor);
        pen.setWidth(std::max(1, strokeWidth));
        painter.setPen(pen);
        painter.drawLine(r.bottomLeft(), r.topRight());
    } else {
        painter.setBrush(buildBrush(r));
        if (strokeWidth > 0) {
            QPen pen(strokeColor);
            pen.setWidth(strokeWidth);
            painter.setPen(pen);
        } else {
            painter.setPen(Qt::NoPen);
        }
        painter.drawPath(buildPath(r));
    }
    painter.end();

    content = img;
    applyTint();
}

std::unique_ptr<Layer> ShapeLayer::clone() const {
    auto copy = std::make_unique<ShapeLayer>(kind, name);
    copy->visible = visible;
    copy->locked = locked;
    copy->opacity = opacity;
    copy->blendMode = blendMode;
    copy->localSize = localSize;
    copy->fillMode = fillMode;
    copy->fillColor = fillColor;
    copy->fillColor2 = fillColor2;
    copy->gradientAngleDeg = gradientAngleDeg;
    copy->strokeColor = strokeColor;
    copy->strokeWidth = strokeWidth;
    copy->cornerRadius = cornerRadius;
    copy->tintColor = tintColor;
    copy->tintAmount = tintAmount;
    copy->transform = transform;
    copy->regenerate();
    return copy;
}
