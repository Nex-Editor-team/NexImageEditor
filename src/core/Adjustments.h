#pragma once
#include "core/Pixels.h"

#include <QColor>

namespace Adjust {
void brightnessContrast(Pixels& p, int brightness, int contrast);  // -100..100
void hueSaturation(Pixels& p, int hue, int saturation, int lightness);  // hue -180..180, sat/light -100..100
void invert(Pixels& p);
void grayscale(Pixels& p);
void tint(Pixels& p, const QColor& color, double amount);  // amount 0..1
void levels(Pixels& p, int black, int white, double gamma);  // black/white 0..255, gamma 0.1..10
}  // namespace Adjust
