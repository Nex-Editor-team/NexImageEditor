#include "core/Pixels.h"

#include <cstring>

QImage newImage(int w, int h, const QColor& fill) {
    QImage img(qMax(1, w), qMax(1, h), kPremult);
    img.fill(fill);
    return img;
}

Pixels toPixels(const QImage& src) {
    QImage img = src.convertToFormat(QImage::Format_RGBA8888);
    Pixels p;
    p.w = img.width();
    p.h = img.height();
    p.d.resize(size_t(p.w) * size_t(p.h) * 4);
    const size_t rowBytes = size_t(p.w) * 4;
    for (int y = 0; y < p.h; ++y)
        std::memcpy(&p.d[size_t(y) * rowBytes], img.constScanLine(y), rowBytes);
    return p;
}

QImage fromPixels(const Pixels& p) {
    QImage img(p.w, p.h, QImage::Format_RGBA8888);
    const size_t rowBytes = size_t(p.w) * 4;
    for (int y = 0; y < p.h; ++y)
        std::memcpy(img.scanLine(y), &p.d[size_t(y) * rowBytes], rowBytes);
    return img.convertToFormat(kPremult);
}
