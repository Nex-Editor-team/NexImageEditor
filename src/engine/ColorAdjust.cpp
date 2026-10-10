#include "ColorAdjust.h"
#include <QColor>
#include <array>
#include <algorithm>
#include <cmath>

namespace {
inline int clamp255(int v) { return std::clamp(v, 0, 255); }

// Runs `fn(r,g,b,a)` over every pixel of a copy of src and returns it.
// Centralizes the "convert -> iterate scanlines -> return" boilerplate
// so each adjustment below is just its own math.
template <typename Fn>
ImageBuffer forEachPixel(const ImageBuffer& src, Fn fn) {
    if (src.isNull()) return {};
    QImage img = src.image().copy();
    img = img.convertToFormat(QImage::Format_ARGB32); // unpremultiply for correct math
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QRgb px = line[x];
            int r = qRed(px), g = qGreen(px), b = qBlue(px), a = qAlpha(px);
            fn(r, g, b, a);
            line[x] = qRgba(clamp255(r), clamp255(g), clamp255(b), a);
        }
    }
    return ImageBuffer(img);
}
} // namespace

namespace ColorAdjust {

ImageBuffer brightnessContrast(const ImageBuffer& src, int brightness, int contrast) {
    // contrast: -100..100 -> factor around 1.0
    double c = (100.0 + contrast) / 100.0;
    c = c * c; // perceptually nicer falloff
    int b = brightness * 255 / 100;

    return forEachPixel(src, [=](int& r, int& g, int& b_, int&) {
        r = static_cast<int>((r - 128) * c + 128) + b;
        g = static_cast<int>((g - 128) * c + 128) + b;
        b_ = static_cast<int>((b_ - 128) * c + 128) + b;
    });
}

ImageBuffer saturation(const ImageBuffer& src, int saturation) {
    double s = std::clamp((saturation + 100) / 100.0, 0.0, 2.0);
    return forEachPixel(src, [=](int& r, int& g, int& b, int&) {
        double gray = 0.299 * r + 0.587 * g + 0.114 * b;
        r = static_cast<int>(gray + (r - gray) * s);
        g = static_cast<int>(gray + (g - gray) * s);
        b = static_cast<int>(gray + (b - gray) * s);
    });
}

ImageBuffer hueShift(const ImageBuffer& src, int degrees) {
    if (src.isNull()) return {};
    QImage img = src.image().copy().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QColor c = QColor::fromRgba(line[x]);
            int h, s, v, a;
            c.getHsv(&h, &s, &v, &a);
            h = (h + degrees) % 360;
            if (h < 0) h += 360;
            c.setHsv(h, s, v, a);
            line[x] = c.rgba();
        }
    }
    return ImageBuffer(img);
}

ImageBuffer levels(const ImageBuffer& src, int blackPoint, int whitePoint, double gamma) {
    blackPoint = std::clamp(blackPoint, 0, 254);
    whitePoint = std::clamp(whitePoint, blackPoint + 1, 255);
    double invGamma = gamma > 0.0 ? 1.0 / gamma : 1.0;

    std::array<int, 256> lut{};
    for (int i = 0; i < 256; ++i) {
        double t = (i - blackPoint) / double(whitePoint - blackPoint);
        t = std::clamp(t, 0.0, 1.0);
        t = std::pow(t, invGamma);
        lut[i] = clamp255(static_cast<int>(t * 255.0));
    }

    return forEachPixel(src, [&](int& r, int& g, int& b, int&) {
        r = lut[r]; g = lut[g]; b = lut[b];
    });
}

} // namespace ColorAdjust
