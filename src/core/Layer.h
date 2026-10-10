#pragma once
#include "core/Geometry.h"
#include "core/Surface.h"

#include <cairo.h>

#include <string>
#include <vector>

enum class BlendMode {
    Normal, Multiply, Screen, Overlay, Darken, Lighten,
    ColorDodge, ColorBurn, HardLight, SoftLight, Difference, Exclusion
};

std::vector<std::string> blendModeNames();
cairo_operator_t blendOperator(BlendMode m);

struct Layer {
    Surface img;             // layer-sized, premultiplied; shared between undo snapshots
    std::string name = "Layer";
    int x = 0;               // canvas position of the image's top-left pixel
    int y = 0;
    bool visible = true;
    bool locked = false;
    bool background = false;
    double opacity = 1.0;    // 0..1
    BlendMode blend = BlendMode::Normal;

    int w() const { return surfW(img); }
    int h() const { return surfH(img); }
    IRect rect() const { return IRect{x, y, w(), h()}; }

    // Copy-on-write: returns the image, cloning it first if an undo snapshot shares it.
    Surface& writable();
};
