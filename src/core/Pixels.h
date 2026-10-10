#pragma once
#include <QColor>
#include <QImage>
#include <cstdint>
#include <vector>

// Layers are stored premultiplied (what QPainter wants). Algorithms work on
// non-premultiplied RGBA8 pixels held in a Pixels buffer.
constexpr QImage::Format kPremult = QImage::Format_ARGB32_Premultiplied;

struct Pixels {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> d;  // R G B A per pixel

    uint8_t* px(int x, int y) { return &d[(size_t(y) * size_t(w) + size_t(x)) * 4]; }
    const uint8_t* px(int x, int y) const { return &d[(size_t(y) * size_t(w) + size_t(x)) * 4]; }
};

QImage newImage(int w, int h, const QColor& fill = QColor(0, 0, 0, 0));
Pixels toPixels(const QImage& img);
QImage fromPixels(const Pixels& p);
