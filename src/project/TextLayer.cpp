#include "TextLayer.h"
#include <QPainter>
#include <QFontMetrics>
#include <algorithm>

TextLayer::TextLayer(QString text, QString name)
    : Layer(LayerType::Text, std::move(name)), text(std::move(text)) {
    regenerate();
}

void TextLayer::regenerate() {
    QFontMetrics fm(font);
    QRect bounds = fm.boundingRect(QRect(0, 0, 4096, 4096), Qt::TextWordWrap, text);
    int w = std::max(1, bounds.width() + 8);
    int h = std::max(1, bounds.height() + 8);

    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(img.rect(), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text);
    painter.end();

    content = img;
    applyTint();
}

std::unique_ptr<Layer> TextLayer::clone() const {
    auto copy = std::make_unique<TextLayer>(text, name);
    copy->visible = visible;
    copy->locked = locked;
    copy->opacity = opacity;
    copy->blendMode = blendMode;
    copy->font = font;
    copy->color = color;
    copy->tintColor = tintColor;
    copy->tintAmount = tintAmount;
    copy->transform = transform;
    copy->regenerate();
    return copy;
}
