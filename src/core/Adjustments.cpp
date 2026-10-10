#include "core/Adjustments.h"

#include <algorithm>
#include <cmath>

namespace {
inline uint8_t clamp8(double v) { return uint8_t(std::clamp(int(std::lround(v)), 0, 255)); }

void rgbToHsl(double r, double g, double b, double& h, double& s, double& l) {
    double mx = std::max({r, g, b}), mn = std::min({r, g, b});
    l = (mx + mn) / 2.0;
    if (mx == mn) { h = s = 0; return; }
    double d = mx - mn;
    s = l > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);
    if (mx == r) h = (g - b) / d + (g < b ? 6 : 0);
    else if (mx == g) h = (b - r) / d + 2;
    else h = (r - g) / d + 4;
    h /= 6.0;
}

double hue2rgb(double p, double q, double t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0 / 6) return p + (q - p) * 6 * t;
    if (t < 0.5) return q;
    if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
    return p;
}

void hslToRgb(double h, double s, double l, double& r, double& g, double& b) {
    if (s == 0) { r = g = b = l; return; }
    double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    double p = 2 * l - q;
    r = hue2rgb(p, q, h + 1.0 / 3);
    g = hue2rgb(p, q, h);
    b = hue2rgb(p, q, h - 1.0 / 3);
}
}  // namespace

namespace Adjust {

void brightnessContrast(Pixels& p, int brightness, int contrast) {
    const double c = contrast * 2.55;
    const double factor = (259.0 * (c + 255.0)) / (255.0 * (259.0 - c));
    const double b = brightness * 2.55;
    for (size_t i = 0; i < p.d.size(); i += 4)
        for (int k = 0; k < 3; ++k) {
            double v = p.d[i + k] + b;
            v = factor * (v - 128) + 128;
            p.d[i + k] = clamp8(v);
        }
}

void hueSaturation(Pixels& p, int hue, int saturation, int lightness) {
    const double dh = hue / 360.0;
    const double ds = saturation / 100.0;
    const double dl = lightness / 100.0;
    for (size_t i = 0; i < p.d.size(); i += 4) {
        double h, s, l, r, g, b;
        rgbToHsl(p.d[i] / 255.0, p.d[i + 1] / 255.0, p.d[i + 2] / 255.0, h, s, l);
        h = std::fmod(h + dh + 1.0, 1.0);
        s = std::clamp(s * (1 + ds), 0.0, 1.0);
        l = std::clamp(dl >= 0 ? l + (1 - l) * dl : l + l * dl, 0.0, 1.0);
        hslToRgb(h, s, l, r, g, b);
        p.d[i] = clamp8(r * 255);
        p.d[i + 1] = clamp8(g * 255);
        p.d[i + 2] = clamp8(b * 255);
    }
}

void invert(Pixels& p) {
    for (size_t i = 0; i < p.d.size(); i += 4)
        for (int k = 0; k < 3; ++k) p.d[i + k] = uint8_t(255 - p.d[i + k]);
}

void grayscale(Pixels& p) {
    for (size_t i = 0; i < p.d.size(); i += 4) {
        double y = 0.299 * p.d[i] + 0.587 * p.d[i + 1] + 0.114 * p.d[i + 2];
        p.d[i] = p.d[i + 1] = p.d[i + 2] = clamp8(y);
    }
}

void tint(Pixels& p, const QColor& color, double amount) {
    amount = std::clamp(amount, 0.0, 1.0);
    for (size_t i = 0; i < p.d.size(); i += 4) {
        double y = (0.299 * p.d[i] + 0.587 * p.d[i + 1] + 0.114 * p.d[i + 2]) / 255.0;
        double tr = color.red() * y, tg = color.green() * y, tb = color.blue() * y;
        p.d[i] = clamp8(p.d[i] * (1 - amount) + tr * amount);
        p.d[i + 1] = clamp8(p.d[i + 1] * (1 - amount) + tg * amount);
        p.d[i + 2] = clamp8(p.d[i + 2] * (1 - amount) + tb * amount);
    }
}

void levels(Pixels& p, int black, int white, double gamma) {
    if (white <= black) white = black + 1;
    uint8_t lut[256];
    for (int v = 0; v < 256; ++v) {
        double t = std::clamp((v - black) / double(white - black), 0.0, 1.0);
        lut[v] = clamp8(std::pow(t, 1.0 / gamma) * 255.0);
    }
    for (size_t i = 0; i < p.d.size(); i += 4)
        for (int k = 0; k < 3; ++k) p.d[i + k] = lut[p.d[i + k]];
}

}  // namespace Adjust
