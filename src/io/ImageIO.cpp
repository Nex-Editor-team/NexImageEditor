#include "ImageIO.h"
#include <QImageReader>
#include <QImageWriter>
#include <QFileInfo>

namespace ImageIO {

ImageBuffer loadImage(const QString& path) {
    QImageReader reader(path);
    reader.setAutoTransform(true); // respect EXIF orientation
    QImage img = reader.read();
    if (img.isNull()) return {};
    return ImageBuffer(img);
}

bool saveImage(const QString& path, const ImageBuffer& buffer, int quality) {
    if (buffer.isNull()) return false;
    QImageWriter writer(path);
    if (quality >= 0) writer.setQuality(quality);
    return writer.write(buffer.image());
}

bool isSupportedExtension(const QString& path) {
    QString ext = QFileInfo(path).suffix().toLower();
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" ||
           ext == "bmp" || ext == "tif" || ext == "tiff";
}

} // namespace ImageIO
