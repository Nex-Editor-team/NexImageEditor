#include "ImageExport.h"
#include <QPainter>
#include <QColorSpace>
#include <opencv2/imgproc.hpp>
#include <algorithm>

bool ImageExport::validSize(QSize size) {
    return size.width() > 0 && size.height() > 0
        && size.width() <= MaxDimension && size.height() <= MaxDimension
        && qint64(size.width()) * size.height() <= MaxPixels;
}

ImageBuffer ImageExport::prepare(const ImageBuffer& source, QSize size, Interpolation interpolation,
                                bool transparent, QColor background) {
    if (source.isNull() || !validSize(size)) return {};
    QImage image = source.image(); // ImageBuffer guarantees ARGB32 premultiplied.
    if (image.size() != size) {
        QImage resized(size, QImage::Format_ARGB32_Premultiplied);
        if (resized.isNull()) return {};
        int mode = cv::INTER_LINEAR;
        switch (interpolation) {
        case Interpolation::Nearest: mode = cv::INTER_NEAREST; break;
        case Interpolation::Bilinear: mode = cv::INTER_LINEAR; break;
        case Interpolation::Bicubic: mode = cv::INTER_CUBIC; break;
        case Interpolation::Lanczos: mode = cv::INTER_LANCZOS4; break;
        case Interpolation::Area: mode = cv::INTER_AREA; break;
        }
        try {
            cv::Mat input(image.height(), image.width(), CV_8UC4,
                          const_cast<uchar*>(image.constBits()), image.bytesPerLine());
            cv::Mat output(resized.height(), resized.width(), CV_8UC4, resized.bits(), resized.bytesPerLine());
            cv::resize(input, output, cv::Size(size.width(), size.height()), 0, 0, mode);
        } catch (const cv::Exception&) { return {}; }
        // Cubic/Lanczos ringing can overshoot alpha: restore premultiplied invariants.
        for (int y = 0; y < resized.height(); ++y) {
            auto* row = reinterpret_cast<QRgb*>(resized.scanLine(y));
            for (int x = 0; x < resized.width(); ++x) {
                const int a = qAlpha(row[x]);
                row[x] = qRgba(std::min(a, qRed(row[x])), std::min(a, qGreen(row[x])),
                               std::min(a, qBlue(row[x])), a);
            }
        }
        resized.setColorSpace(image.colorSpace());
        image = resized;
    }
    if (!transparent) {
        QImage opaque(size, QImage::Format_ARGB32_Premultiplied);
        if (opaque.isNull()) return {};
        background.setAlpha(255);
        opaque.fill(background);
        QPainter painter(&opaque);
        painter.drawImage(0, 0, image);
        painter.end();
        opaque.setColorSpace(image.colorSpace());
        image = opaque;
    }
    return ImageBuffer(image);
}
