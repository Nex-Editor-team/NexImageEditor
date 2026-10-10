#include "core/Layer.h"

std::vector<std::string> blendModeNames() {
    return {"Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten", "Color Dodge",
            "Color Burn", "Hard Light", "Soft Light", "Difference", "Exclusion"};
}

cairo_operator_t blendOperator(BlendMode m) {
    switch (m) {
    case BlendMode::Multiply: return CAIRO_OPERATOR_MULTIPLY;
    case BlendMode::Screen: return CAIRO_OPERATOR_SCREEN;
    case BlendMode::Overlay: return CAIRO_OPERATOR_OVERLAY;
    case BlendMode::Darken: return CAIRO_OPERATOR_DARKEN;
    case BlendMode::Lighten: return CAIRO_OPERATOR_LIGHTEN;
    case BlendMode::ColorDodge: return CAIRO_OPERATOR_COLOR_DODGE;
    case BlendMode::ColorBurn: return CAIRO_OPERATOR_COLOR_BURN;
    case BlendMode::HardLight: return CAIRO_OPERATOR_HARD_LIGHT;
    case BlendMode::SoftLight: return CAIRO_OPERATOR_SOFT_LIGHT;
    case BlendMode::Difference: return CAIRO_OPERATOR_DIFFERENCE;
    case BlendMode::Exclusion: return CAIRO_OPERATOR_EXCLUSION;
    case BlendMode::Normal: break;
    }
    return CAIRO_OPERATOR_OVER;
}

Surface& Layer::writable() {
    if (img.use_count() > 1) img = cloneSurface(img);
    return img;
}
