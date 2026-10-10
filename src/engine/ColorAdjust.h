#pragma once
#include "ImageBuffer.h"

// All parameters are in the -100..100 "user friendly" range unless noted.
namespace ColorAdjust {
    ImageBuffer brightnessContrast(const ImageBuffer& src, int brightness, int contrast);
    ImageBuffer saturation(const ImageBuffer& src, int saturation); // -100 (gray) .. 100 (vivid)
    ImageBuffer hueShift(const ImageBuffer& src, int degrees);      // -180..180
    ImageBuffer levels(const ImageBuffer& src, int blackPoint, int whitePoint, double gamma);
}
