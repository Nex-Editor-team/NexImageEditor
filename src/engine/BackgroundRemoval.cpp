#include "BackgroundRemoval.h"
#include <cmath>
#include <algorithm>

namespace BackgroundRemoval {

ImageBuffer removeByColor(const ImageBuffer& src, const QColor& bgColor, int tolerancePercent) {
    if (src.isNull()) return {};

    QImage img = src.image().convertToFormat(QImage::Format_ARGB32);
    int tr = bgColor.red(), tg = bgColor.green(), tb = bgColor.blue();

    // Max possible Euclidean RGB distance is sqrt(255^2*3) ~= 441.67.
    constexpr double kMaxDistance = 441.6729559;
    double tol = std::clamp(tolerancePercent, 0, 100) / 100.0 * kMaxDistance;
    double feather = std::max(8.0, tol * 0.25); // soft edge band beyond the hard threshold

    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QRgb px = line[x];
            int r = qRed(px), g = qGreen(px), b = qBlue(px), a = qAlpha(px);

            double dr = r - tr, dg = g - tg, db = b - tb;
            double dist = std::sqrt(dr * dr + dg * dg + db * db);

            double alphaScale;
            if (dist <= tol) alphaScale = 0.0;
            else if (dist >= tol + feather) alphaScale = 1.0;
            else alphaScale = (dist - tol) / feather;

            int newAlpha = int(a * alphaScale);
            line[x] = qRgba(r, g, b, newAlpha);
        }
    }
    return ImageBuffer(img);
}

} // namespace BackgroundRemoval
