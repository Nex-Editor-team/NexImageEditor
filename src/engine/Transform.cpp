#include "Transform.h"
#include <QPainter>
#include <QTransform>
#include <QtMath>

namespace Transform {

ImageBuffer resize(const ImageBuffer& src, int newWidth, int newHeight, ResizeMode mode) {
    if (src.isNull() || newWidth <= 0 || newHeight <= 0) return {};
    Qt::TransformationMode tm = (mode == ResizeMode::Bilinear) ? Qt::SmoothTransformation
                                                                : Qt::FastTransformation;
    QImage out = src.image().scaled(newWidth, newHeight, Qt::IgnoreAspectRatio, tm);
    return ImageBuffer(out);
}

ImageBuffer crop(const ImageBuffer& src, const QRect& rect) {
    if (src.isNull()) return {};
    QRect clamped = rect.intersected(src.rect());
    if (clamped.isEmpty()) return {};
    return ImageBuffer(src.image().copy(clamped));
}

ImageBuffer rotate90(const ImageBuffer& src, bool clockwise) {
    if (src.isNull()) return {};
    QTransform t;
    t.rotate(clockwise ? 90.0 : -90.0);
    return ImageBuffer(src.image().transformed(t, Qt::SmoothTransformation));
}

ImageBuffer flipHorizontal(const ImageBuffer& src) {
    if (src.isNull()) return {};
    return ImageBuffer(src.image().mirrored(true, false));
}

ImageBuffer flipVertical(const ImageBuffer& src) {
    if (src.isNull()) return {};
    return ImageBuffer(src.image().mirrored(false, true));
}

QTransform FreeTransformState::toQTransform(const QSizeF& contentSize) const {
    const double hw = contentSize.width() / 2.0;
    const double hh = contentSize.height() / 2.0;

    QTransform t;
    // Move content's own center to the target canvas position first (we
    // build right-to-left: this is applied LAST to the already
    // scaled/sheared/rotated content).
    t.translate(center.x(), center.y());
    t.rotate(rotationDeg);
    t.shear(qTan(qDegreesToRadians(shearXDeg)), 0.0);
    t.scale(scaleX, scaleY);
    // Now shift so the content's local center (hw, hh) lands at the
    // origin of the transform chain above, i.e. content is authored in
    // [0..w, 0..h] local space.
    t.translate(-hw, -hh);
    return t;
}

ImageBuffer applyFreeTransform(const ImageBuffer& src, const FreeTransformState& state, const QSize& outputSize) {
    if (src.isNull() || outputSize.isEmpty()) return {};

    QImage out(outputSize, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);

    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setTransform(state.toQTransform(src.size()));
    painter.drawImage(QPoint(0, 0), src.image());
    painter.end();

    return ImageBuffer(out);
}

} // namespace Transform
