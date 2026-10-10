#include "core/Adjustments.h"

#include "core/Pixels.h"

#include <algorithm>
#include <cmath>

namespace adjust {
namespace {

uint8_t clamp8(double v) { return uint8_t(std::clamp(std::lround(v), 0L, 255L)); }

template <typename F>
Surface mapPixels(const Surface& src, F fn) {
    Pixels p = toPixels(src);
    for (size_t i = 0; i < p.d.size(); i += 4) fn(&p.d[i]);
    return fromPixels(p);
}

void rgbToHsl(double r, double g, double b, double& h, double& s, double& l) {
    double mx = std::max({r, g, b}), mn = std::min({r, g, b});
    l = (mx + mn) / 2;
    if (mx == mn) {
        h = s = 0;
        return;
    }
    double d = mx - mn;
    s = l > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
    if (mx == r) h = (g - b) / d + (g < b ? 6 : 0);
    else if (mx == g) h = (b - r) / d + 2;
    else h = (r - g) / d + 4;
    h /= 6;
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
    if (s == 0) {
        r = g = b = l;
        return;
    }
    double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    double p = 2 * l - q;
    r = hue2rgb(p, q, h + 1.0 / 3);
    g = hue2rgb(p, q, h);
    b = hue2rgb(p, q, h - 1.0 / 3);
}

}  // namespace

Surface invert(const Surface& src) {
    return mapPixels(src, [](uint8_t* p) {
        p[0] = 255 - p[0];
        p[1] = 255 - p[1];
        p[2] = 255 - p[2];
    });
}

Surface grayscale(const Surface& src) {
    return mapPixels(src, [](uint8_t* p) {
        uint8_t y = clamp8(0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]);
        p[0] = p[1] = p[2] = y;
    });
}

Surface brightnessContrast(const Surface& src, int brightness, int contrast) {
    const double b = brightness * 2.55;
    const double c = contrast / 100.0;
    const double k = c >= 0 ? 1.0 / std::max(0.01, 1 - c) : 1 + c;
    return mapPixels(src, [&](uint8_t* p) {
        for (int k2 = 0; k2 < 3; ++k2) {
            double v = (p[k2] + b - 128) * k + 128;
            p[k2] = clamp8(v);
        }
    });
}

Surface hueSaturation(const Surface& src, int hue, int saturation, int lightness) {
    const double dh = hue / 360.0, ds = saturation / 100.0, dl = lightness / 100.0;
    return mapPixels(src, [&](uint8_t* p) {
        double h, s, l;
        rgbToHsl(p[0] / 255.0, p[1] / 255.0, p[2] / 255.0, h, s, l);
        h = std::fmod(h + dh + 1.0, 1.0);
        s = std::clamp(s * (1 + ds), 0.0, 1.0);
        l = std::clamp(dl >= 0 ? l + (1 - l) * dl : l + l * dl, 0.0, 1.0);
        double r, g, bb;
        hslToRgb(h, s, l, r, g, bb);
        p[0] = clamp8(r * 255);
        p[1] = clamp8(g * 255);
        p[2] = clamp8(bb * 255);
    });
}

Surface threshold(const Surface& src, int level) {
    return mapPixels(src, [&](uint8_t* p) {
        double y = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
        uint8_t v = y >= level ? 255 : 0;
        p[0] = p[1] = p[2] = v;
    });
}

Surface posterize(const Surface& src, int levels) {
    const double step = 255.0 / (levels - 1);
    return mapPixels(src, [&](uint8_t* p) {
        for (int k = 0; k < 3; ++k) p[k] = clamp8(std::round(p[k] / step) * step);
    });
}

}  // namespace adjust
