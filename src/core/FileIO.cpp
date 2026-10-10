#include "core/FileIO.h"

#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>

namespace {
constexpr quint32 kMagic = 0x4E584531;  // "NXE1"
constexpr quint32 kVersion = 1;

void setError(QString* error, const QString& msg) {
    if (error) *error = msg;
}
}  // namespace

namespace FileIO {

QString imageOpenFilter() {
    return "Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff *.nxe);;"
           "PNG (*.png);;JPEG (*.jpg *.jpeg);;WebP (*.webp);;BMP (*.bmp);;TIFF (*.tif *.tiff);;"
           "NexImage project (*.nxe)";
}

QString exportFilter() {
    return "PNG (*.png);;JPEG (*.jpg *.jpeg);;WebP (*.webp);;BMP (*.bmp);;TIFF (*.tif *.tiff)";
}

bool importImage(const QString& path, Document& out, QString* error) {
    if (QFileInfo(path).suffix().compare("nxe", Qt::CaseInsensitive) == 0)
        return loadProject(path, out, error);
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) {
        setError(error, reader.errorString());
        return false;
    }
    out = Document(img.width(), img.height(), Qt::transparent, QFileInfo(path).completeBaseName());
    out.layers()[0].image = img.convertToFormat(kPremult);
    out.layers()[0].name = "Background";
    out.filePath.clear();
    out.dirty = false;
    return true;
}

bool exportImage(const Document& doc, const QString& path, int quality, QString* error) {
    QImage img = doc.flattenedVisible();
    QString fmt = QFileInfo(path).suffix().toLower();
    if (fmt == "jpg") fmt = "jpeg";
    if (fmt == "tif") fmt = "tiff";
    // JPEG and BMP have no alpha: flatten onto white.
    if (fmt == "jpeg" || fmt == "bmp") {
        QImage flat(img.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        QPainter p(&flat);
        p.drawImage(0, 0, img);
        p.end();
        img = flat;
    }
    QImageWriter writer(path, fmt.toLatin1());
    writer.setQuality(quality);
    if (!writer.write(img)) {
        setError(error, writer.errorString());
        return false;
    }
    return true;
}

bool saveProject(const Document& doc, const QString& path, QString* error) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        setError(error, f.errorString());
        return false;
    }
    QDataStream s(&f);
    s.setVersion(QDataStream::Qt_6_0);
    s << kMagic << kVersion << doc.width() << doc.height() << doc.activeIndex();
    s << quint32(doc.layers().size());
    for (const Layer& l : doc.layers()) {
        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        l.image.save(&buf, "PNG");
        s << l.name << l.x << l.y << l.visible << l.locked << l.background << l.opacity
          << int(l.blend) << png;
    }
    if (s.status() != QDataStream::Ok) {
        setError(error, "write failed");
        return false;
    }
    return true;
}

bool loadProject(const QString& path, Document& out, QString* error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        setError(error, f.errorString());
        return false;
    }
    QDataStream s(&f);
    s.setVersion(QDataStream::Qt_6_0);
    quint32 magic = 0, version = 0, count = 0;
    int w = 0, h = 0, active = 0;
    s >> magic >> version;
    if (magic != kMagic || version != kVersion) {
        setError(error, "Not a NexImage project file");
        return false;
    }
    s >> w >> h >> active >> count;
    if (s.status() != QDataStream::Ok || w <= 0 || h <= 0 || count == 0 || count > 1000) {
        setError(error, "Corrupt project header");
        return false;
    }
    Document doc(w, h, Qt::transparent, QFileInfo(path).completeBaseName());
    doc.layers().clear();
    for (quint32 i = 0; i < count; ++i) {
        Layer l;
        int blend = 0;
        QByteArray png;
        s >> l.name >> l.x >> l.y >> l.visible >> l.locked >> l.background >> l.opacity >> blend >> png;
        if (s.status() != QDataStream::Ok) {
            setError(error, "Corrupt layer data");
            return false;
        }
        l.image = QImage::fromData(png, "PNG").convertToFormat(kPremult);
        l.blend = BlendMode(blend);
        doc.layers().push_back(std::move(l));
    }
    doc.setActive(active);
    doc.filePath = path;
    out = std::move(doc);
    return true;
}

}  // namespace FileIO
