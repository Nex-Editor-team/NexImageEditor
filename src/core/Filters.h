#pragma once
#include "core/Pixels.h"

namespace Filters {
void gaussianBlur(Pixels& p, double radius);
void unsharpMask(Pixels& p, double radius, double amount, int threshold);
void median(Pixels& p, int radius);
void addNoise(Pixels& p, double amount, bool monochrome, unsigned seed = 1234);
}  // namespace Filters
