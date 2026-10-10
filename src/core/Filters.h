#pragma once
#include "core/Layer.h"

// Neighbourhood filters. Each returns a new layer-sized surface; apply with ops::applyResult.
namespace filters {
Surface gaussianBlur(const Surface& src, double radius);
Surface unsharpMask(const Surface& src, double radius, double amount, int threshold);  // amount 0..500 %
Surface median(const Surface& src, int radius);
Surface addNoise(const Surface& src, double amount, bool monochrome, unsigned seed);  // amount 0..100
}
