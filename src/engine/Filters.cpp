#include "Filters.h"
#include <algorithm>
#include <vector>

namespace {
inline int clamp255(int v) { return std::clamp(v, 0, 255); }
}

namespace Filters {

ImageBuffer grayscale(const ImageBuffer& src) {
    if (src.isNull()) return {};
    QImage img = src.image().copy().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QRgb px = line[x];
            int gray = static_cast<int>(0.299 * qRed(px) + 0.587 * qGreen(px) + 0.114 * qBlue(px));
            line[x] = qRgba(gray, gray, gray, qAlpha(px));
        }
    }
    return ImageBuffer(img);
}

ImageBuffer invert(const ImageBuffer& src) {
    if (src.isNull()) return {};
    QImage img = src.image().copy();
    img.invertPixels(QImage::InvertRgb);
    return ImageBuffer(img);
}

ImageBuffer sepia(const ImageBuffer& src) {
    if (src.isNull()) return {};
    QImage img = src.image().copy().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QRgb px = line[x];
            int r = qRed(px), g = qGreen(px), b = qBlue(px), a = qAlpha(px);
            int nr = clamp255(int(0.393 * r + 0.769 * g + 0.189 * b));
            int ng = clamp255(int(0.349 * r + 0.686 * g + 0.168 * b));
            int nb = clamp255(int(0.272 * r + 0.534 * g + 0.131 * b));
            line[x] = qRgba(nr, ng, nb, a);
        }
    }
    return ImageBuffer(img);
}

// Simple separable box blur (two passes: horizontal then vertical).
// Not gaussian-quality but fast and dependency-free, which fits a
// "lightweight" editor.
ImageBuffer boxBlur(const ImageBuffer& src, int radius) {
    if (src.isNull() || radius <= 0) return src;
    QImage img = src.image().copy().convertToFormat(QImage::Format_ARGB32);
    const int w = img.width(), h = img.height();
    QImage tmp = img;

    auto blurPass = [&](const QImage& in, QImage& out, bool horizontal) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                long sr = 0, sg = 0, sb = 0, sa = 0, count = 0;
                for (int k = -radius; k <= radius; ++k) {
                    int sx = horizontal ? x + k : x;
                    int sy = horizontal ? y : y + k;
                    if (sx < 0 || sx >= w || sy < 0 || sy >= h) continue;
                    QRgb px = in.pixel(sx, sy);
                    sr += qRed(px); sg += qGreen(px); sb += qBlue(px); sa += qAlpha(px);
                    ++count;
                }
                out.setPixel(x, y, qRgba(int(sr / count), int(sg / count), int(sb / count), int(sa / count)));
            }
        }
    };

    blurPass(img, tmp, true);
    QImage out = tmp;
    blurPass(tmp, out, false);
    return ImageBuffer(out);
}

// Unsharp mask: original + amount * (original - blurred).
ImageBuffer sharpen(const ImageBuffer& src, double amount) {
    if (src.isNull()) return {};
    ImageBuffer blurred = boxBlur(src, 2);
    QImage orig = src.image().convertToFormat(QImage::Format_ARGB32);
    QImage blur = blurred.image().convertToFormat(QImage::Format_ARGB32);
    QImage out = orig.copy();

    for (int y = 0; y < out.height(); ++y) {
        for (int x = 0; x < out.width(); ++x) {
            QRgb o = orig.pixel(x, y);
            QRgb b = blur.pixel(x, y);
            int r = clamp255(int(qRed(o) + amount * (qRed(o) - qRed(b))));
            int g = clamp255(int(qGreen(o) + amount * (qGreen(o) - qGreen(b))));
            int bch = clamp255(int(qBlue(o) + amount * (qBlue(o) - qBlue(b))));
            out.setPixel(x, y, qRgba(r, g, bch, qAlpha(o)));
        }
    }
    return ImageBuffer(out);
}

} // namespace Filters
