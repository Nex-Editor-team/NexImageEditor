#pragma once
#include <QImage>
#include <QColor>
#include <QString>

// Thin, format-stable wrapper around a raw RGBA pixel buffer.
// Internally backed by QImage (Format_ARGB32_Premultiplied) so the rest
// of the engine has one guaranteed pixel layout to reason about, instead
// of every call site having to worry about QImage's many formats.
class ImageBuffer {
public:
    ImageBuffer() = default;
    explicit ImageBuffer(const QImage& img);
    ImageBuffer(int width, int height, const QColor& fill = Qt::transparent);

    bool isNull() const { return m_image.isNull(); }
    int width() const { return m_image.width(); }
    int height() const { return m_image.height(); }
    QSize size() const { return m_image.size(); }
    QRect rect() const { return m_image.rect(); }

    const QImage& image() const { return m_image; }
    QImage& image() { return m_image; }
    void setImage(const QImage& img);

    QRgb pixel(int x, int y) const { return m_image.pixel(x, y); }
    void setPixel(int x, int y, QRgb value) { m_image.setPixel(x, y, value); }

    static ImageBuffer load(const QString& path);
    bool save(const QString& path, int quality = -1) const;

private:
    QImage m_image;
};
