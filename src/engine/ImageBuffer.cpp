#include "ImageBuffer.h"

ImageBuffer::ImageBuffer(const QImage& img) {
    setImage(img);
}

ImageBuffer::ImageBuffer(int width, int height, const QColor& fill) {
    QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
    img.fill(fill);
    m_image = img;
}

void ImageBuffer::setImage(const QImage& img) {
    m_image = img.format() == QImage::Format_ARGB32_Premultiplied
                  ? img
                  : img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

ImageBuffer ImageBuffer::load(const QString& path) {
    QImage img(path);
    return ImageBuffer(img); // isNull() reports failure
}

bool ImageBuffer::save(const QString& path, int quality) const {
    if (m_image.isNull()) return false;
    return m_image.save(path, nullptr, quality);
}
