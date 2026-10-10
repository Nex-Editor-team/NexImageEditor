#pragma once
#include <QPainter>
#include <QString>

// Layer blend modes. Qt's QPainter already implements correct alpha
// compositing for all of these via CompositionMode, so we simply map
// onto that instead of hand-rolling per-pixel blending (fast + correct).
enum class BlendMode {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    Difference,
    ColorDodge,
    ColorBurn
};

namespace Blend {
    inline QPainter::CompositionMode toCompositionMode(BlendMode mode) {
        switch (mode) {
            case BlendMode::Normal:      return QPainter::CompositionMode_SourceOver;
            case BlendMode::Multiply:    return QPainter::CompositionMode_Multiply;
            case BlendMode::Screen:      return QPainter::CompositionMode_Screen;
            case BlendMode::Overlay:     return QPainter::CompositionMode_Overlay;
            case BlendMode::Darken:      return QPainter::CompositionMode_Darken;
            case BlendMode::Lighten:     return QPainter::CompositionMode_Lighten;
            case BlendMode::Difference:  return QPainter::CompositionMode_Difference;
            case BlendMode::ColorDodge:  return QPainter::CompositionMode_ColorDodge;
            case BlendMode::ColorBurn:   return QPainter::CompositionMode_ColorBurn;
        }
        return QPainter::CompositionMode_SourceOver;
    }

    inline QString toString(BlendMode mode) {
        switch (mode) {
            case BlendMode::Normal:      return "Normal";
            case BlendMode::Multiply:    return "Multiply";
            case BlendMode::Screen:      return "Screen";
            case BlendMode::Overlay:     return "Overlay";
            case BlendMode::Darken:      return "Darken";
            case BlendMode::Lighten:     return "Lighten";
            case BlendMode::Difference:  return "Difference";
            case BlendMode::ColorDodge:  return "Color Dodge";
            case BlendMode::ColorBurn:   return "Color Burn";
        }
        return "Normal";
    }

    inline BlendMode fromString(const QString& s) {
        if (s == "Multiply") return BlendMode::Multiply;
        if (s == "Screen") return BlendMode::Screen;
        if (s == "Overlay") return BlendMode::Overlay;
        if (s == "Darken") return BlendMode::Darken;
        if (s == "Lighten") return BlendMode::Lighten;
        if (s == "Difference") return BlendMode::Difference;
        if (s == "Color Dodge") return BlendMode::ColorDodge;
        if (s == "Color Burn") return BlendMode::ColorBurn;
        return BlendMode::Normal;
    }
}
