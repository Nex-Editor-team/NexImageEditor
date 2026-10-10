#pragma once
#include "ImageBuffer.h"

namespace Filters {
    ImageBuffer grayscale(const ImageBuffer& src);
    ImageBuffer invert(const ImageBuffer& src);
    ImageBuffer gaussianBlur(const ImageBuffer& src, double sigma);
    ImageBuffer boxBlur(const ImageBuffer& src, int radius);
    ImageBuffer sharpen(const ImageBuffer& src, double amount = 1.0);
    ImageBuffer sepia(const ImageBuffer& src);
}
