#include "ImageIO.h"
#include <QImageReader>
#include <QImageWriter>
#include <QFileInfo>
#include <QSaveFile>

namespace ImageIO {

ImageBuffer loadImage(const QString& path) {
    QImageReader reader(path);
    reader.setAutoTransform(true); // respect EXIF orientation
    QImage img = reader.read();
    if (img.isNull()) return {};
    return ImageBuffer(img);
}

bool saveImage(const QString& path, const ImageBuffer& buffer, int quality, const QByteArray& format) {
    if (buffer.isNull()) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QByteArray actualFormat = format.isEmpty() ? QFileInfo(path).suffix().toLatin1() : format;
    QImageWriter writer(&file, actualFormat);
    if (quality >= 0) writer.setQuality(quality);
    if (!writer.write(buffer.image())) return false;
    return file.commit();
}

bool isSupportedExtension(const QString& path) {
    QString ext = QFileInfo(path).suffix().toLower();
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" ||
           ext == "bmp" || ext == "tif" || ext == "tiff";
}

} // namespace ImageIO
