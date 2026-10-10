#pragma once
#include "ImageBuffer.h"
#include <QColor>

// Simple, dependency-free background removal for a "lightweight"
// editor: no ML/segmentation, just a color-distance threshold with a
// soft-edge feather so the cutout doesn't look jagged. Works well for
// flat/solid-color backgrounds (studio photos, screenshots, etc.);
// it is not a substitute for AI-based subject segmentation.
namespace BackgroundRemoval {

// `tolerancePercent` (0..100) controls how close a pixel's color must
// be to `bgColor` to be considered background. Pixels within the
// tolerance become fully transparent; pixels in a soft feather band
// just outside it are partially transparent for a smoother edge.
ImageBuffer removeByColor(const ImageBuffer& src, const QColor& bgColor, int tolerancePercent);

} // namespace BackgroundRemoval
