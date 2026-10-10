#include "core/Filters.h"

#include "core/Pixels.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace filters {
namespace {

uint8_t clamp8(double v) { return uint8_t(std::clamp(std::lround(v), 0L, 255L)); }

// Separable convolution on non-premultiplied RGBA with edge clamping.
Pixels convolve(const Pixels& in, const std::vector<double>& k) {
    const int r = int(k.size() / 2);
    Pixels tmp = in, out = in;
    for (int y = 0; y < in.h; ++y)
        for (int x = 0; x < in.w; ++x)
            for (int c = 0; c < 4; ++c) {
                double acc = 0;
                for (int i = -r; i <= r; ++i)
                    acc += k[size_t(i + r)] * in.px(std::clamp(x + i, 0, in.w - 1), y)[c];
                tmp.px(x, y)[c] = clamp8(acc);
            }
    for (int y = 0; y < in.h; ++y)
        for (int x = 0; x < in.w; ++x)
            for (int c = 0; c < 4; ++c) {
                double acc = 0;
                for (int i = -r; i <= r; ++i)
                    acc += k[size_t(i + r)] * tmp.px(x, std::clamp(y + i, 0, in.h - 1))[c];
                out.px(x, y)[c] = clamp8(acc);
            }
    return out;
}

std::vector<double> gaussKernel(double sigma) {
    if (sigma <= 0) return {1.0};
    const int r = std::max(1, int(std::ceil(sigma * 3)));
    std::vector<double> k(size_t(2 * r + 1));
    double sum = 0;
    for (int i = -r; i <= r; ++i) {
        k[size_t(i + r)] = std::exp(-(i * i) / (2 * sigma * sigma));
        sum += k[size_t(i + r)];
    }
    for (double& v : k) v /= sum;
    return k;
}

}  // namespace

Surface gaussianBlur(const Surface& src, double radius) {
    Pixels p = toPixels(src);
    return fromPixels(convolve(p, gaussKernel(radius)));
}

Surface unsharpMask(const Surface& src, double radius, double amount, int threshold) {
    Pixels orig = toPixels(src);
    Pixels blur = convolve(orig, gaussKernel(radius));
    const double a = amount / 100.0;
    for (size_t i = 0; i < orig.d.size(); i += 4)
        for (int c = 0; c < 3; ++c) {
            int diff = int(orig.d[i + c]) - int(blur.d[i + c]);
            if (std::abs(diff) < threshold) continue;
            orig.d[i + c] = clamp8(orig.d[i + c] + a * diff);
        }
    return fromPixels(orig);
}

Surface median(const Surface& src, int radius) {
    Pixels in = toPixels(src);
    Pixels out = in;
    std::vector<uint8_t> win;
    win.reserve(size_t((2 * radius + 1) * (2 * radius + 1)));
    for (int y = 0; y < in.h; ++y)
        for (int x = 0; x < in.w; ++x)
            for (int c = 0; c < 4; ++c) {
                win.clear();
                for (int j = -radius; j <= radius; ++j)
                    for (int i = -radius; i <= radius; ++i)
                        win.push_back(in.px(std::clamp(x + i, 0, in.w - 1), std::clamp(y + j, 0, in.h - 1))[c]);
                auto mid = win.begin() + win.size() / 2;
                std::nth_element(win.begin(), mid, win.end());
                out.px(x, y)[c] = *mid;
            }
    return fromPixels(out);
}

Surface addNoise(const Surface& src, double amount, bool monochrome, unsigned seed) {
    Pixels p = toPixels(src);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    const double span = amount / 100.0 * 255.0;
    for (size_t i = 0; i < p.d.size(); i += 4) {
        const double mono = dist(rng) * span;
        for (int c = 0; c < 3; ++c) {
            double n = monochrome ? mono : dist(rng) * span;
            p.d[i + c] = clamp8(p.d[i + c] + n);
        }
    }
    return fromPixels(p);
}

}  // namespace filters
