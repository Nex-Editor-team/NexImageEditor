#pragma once
#include <QImage>
#include <QPainter>
#include <QRect>
#include <QString>
#include <QStringList>

enum class BlendMode {
    Normal, Multiply, Screen, Overlay, Darken, Lighten,
    ColorDodge, ColorBurn, HardLight, SoftLight, Difference, Exclusion
};

inline QStringList blendModeNames() {
    return {"Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten", "Color Dodge",
            "Color Burn", "Hard Light", "Soft Light", "Difference", "Exclusion"};
}

inline QPainter::CompositionMode blendComposition(BlendMode m) {
    switch (m) {
    case BlendMode::Multiply: return QPainter::CompositionMode_Multiply;
    case BlendMode::Screen: return QPainter::CompositionMode_Screen;
    case BlendMode::Overlay: return QPainter::CompositionMode_Overlay;
    case BlendMode::Darken: return QPainter::CompositionMode_Darken;
    case BlendMode::Lighten: return QPainter::CompositionMode_Lighten;
    case BlendMode::ColorDodge: return QPainter::CompositionMode_ColorDodge;
    case BlendMode::ColorBurn: return QPainter::CompositionMode_ColorBurn;
    case BlendMode::HardLight: return QPainter::CompositionMode_HardLight;
    case BlendMode::SoftLight: return QPainter::CompositionMode_SoftLight;
    case BlendMode::Difference: return QPainter::CompositionMode_Difference;
    case BlendMode::Exclusion: return QPainter::CompositionMode_Exclusion;
    case BlendMode::Normal: break;
    }
    return QPainter::CompositionMode_SourceOver;
}

struct Layer {
    QImage image;  // premultiplied ARGB32
    QString name = "Layer";
    int x = 0;     // position of the image's top-left corner on the canvas
    int y = 0;
    bool visible = true;
    bool locked = false;
    bool background = false;
    double opacity = 1.0;  // 0..1
    BlendMode blend = BlendMode::Normal;

    int w() const { return image.width(); }
    int h() const { return image.height(); }
    QRect rect() const { return QRect(x, y, w(), h()); }
};
