#include "Layer.h"
#include "../engine/PhotoEngine.h"
#include <QPainter>
#include <algorithm>

Layer::Layer(LayerType type, QString name)
    : name(std::move(name)), m_id(QUuid::createUuid()), m_type(type) {}

QRectF Layer::boundingRect() const {
    if (content.isNull()) return {};
    return matrix().mapRect(QRectF(QPointF(0, 0), QSizeF(content.size())));
}

void Layer::applyAdjustments() {
    if (adjustments.isIdentity() || content.isNull()) {
        m_adjustedContent = QImage();
    } else {
        m_adjustedContent = PhotoEngine::apply(ImageBuffer(content), adjustments).image();
    }
    applyTint();
}

void Layer::bakeAdjustments() {
    if (!m_adjustedContent.isNull()) {
        content = m_adjustedContent;
        m_adjustedContent = QImage();
        adjustments.reset();
        applyTint();
    }
}

void Layer::applyTint() {
    const QImage& base = (!m_adjustedContent.isNull()) ? m_adjustedContent : content;
    if (tintAmount <= 0.0 || base.isNull()) {
        m_tintedContent = QImage();
        return;
    }
    double amt = std::clamp(tintAmount, 0.0, 1.0);
    QImage img = base.convertToFormat(QImage::Format_ARGB32);
    int tr = tintColor.red(), tg = tintColor.green(), tb = tintColor.blue();

    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QRgb px = line[x];
            int a = qAlpha(px);
            int r = int(qRed(px) * (1.0 - amt) + tr * amt);
            int g = int(qGreen(px) * (1.0 - amt) + tg * amt);
            int b = int(qBlue(px) * (1.0 - amt) + tb * amt);
            line[x] = qRgba(r, g, b, a);
        }
    }
    m_tintedContent = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

void Layer::render(QPainter& painter) const {
    if (!visible || content.isNull() || opacity <= 0.0) return;

    painter.save();
    painter.setOpacity(opacity);
    painter.setCompositionMode(Blend::toCompositionMode(blendMode));
    painter.setTransform(matrix(), true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QPoint(0, 0), displayContent());
    painter.restore();
}

std::unique_ptr<Layer> Layer::clone() const {
    auto copy = std::make_unique<Layer>(m_type, name);
    copy->visible = visible;
    copy->locked = locked;
    copy->opacity = opacity;
    copy->blendMode = blendMode;
    copy->content = content;
    copy->adjustments = adjustments;
    copy->tintColor = tintColor;
    copy->tintAmount = tintAmount;
    copy->transform = transform;
    copy->applyAdjustments();
    return copy;
}
