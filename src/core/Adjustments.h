#pragma once
#include "core/Layer.h"
#include "core/Selection.h"

// Per-pixel colour adjustments. Each returns a new layer-sized surface; apply with ops::applyResult.
namespace adjust {
Surface invert(const Surface& src);
Surface grayscale(const Surface& src);
Surface brightnessContrast(const Surface& src, int brightness, int contrast);  // -100..100
Surface hueSaturation(const Surface& src, int hue, int saturation, int lightness);  // hue -180..180, sat/light -100..100
Surface threshold(const Surface& src, int level);  // 1..255
Surface posterize(const Surface& src, int levels);  // 2..255
}
