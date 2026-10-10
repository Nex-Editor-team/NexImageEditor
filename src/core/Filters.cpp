#include "core/Filters.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {
inline uint8_t clamp8(double v) { return uint8_t(std::clamp(int(std::lround(v)), 0, 255)); }

std::vector<double> gaussKernel(double sigma, int& radius) {
    radius = std::max(1, int(std::ceil(sigma * 3)));
    std::vector<double> k(size_t(2 * radius + 1));
    double sum = 0;
    for (int i = -radius; i <= radius; ++i) {
        k[size_t(i + radius)] = std::exp(-(i * i) / (2 * sigma * sigma));
        sum += k[size_t(i + radius)];
    }
    for (double& v : k) v /= sum;
    return k;
}

// Separable blur on RGB channels; alpha is handled as a channel too (premult not needed for
// the small radii used here).
std::vector<uint8_t> blurBuffer(const Pixels& p, double sigma) {
    int r = 0;
    auto k = gaussKernel(sigma, r);
    const int W = p.w, H = p.h;
    std::vector<double> tmp(size_t(W) * H * 4);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            for (int c = 0; c < 4; ++c) {
                double acc = 0;
                for (int i = -r; i <= r; ++i) {
                    int sx = std::clamp(x + i, 0, W - 1);
                    acc += k[size_t(i + r)] * p.d[(size_t(y) * W + sx) * 4 + c];
                }
                tmp[(size_t(y) * W + x) * 4 + c] = acc;
            }
    std::vector<uint8_t> out(p.d.size());
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            for (int c = 0; c < 4; ++c) {
                double acc = 0;
                for (int i = -r; i <= r; ++i) {
                    int sy = std::clamp(y + i, 0, H - 1);
                    acc += k[size_t(i + r)] * tmp[(size_t(sy) * W + x) * 4 + c];
                }
                out[(size_t(y) * W + x) * 4 + c] = clamp8(acc);
            }
    return out;
}
}  // namespace

namespace Filters {

void gaussianBlur(Pixels& p, double radius) {
    if (radius <= 0 || p.w == 0) return;
    p.d = blurBuffer(p, radius);
}

void unsharpMask(Pixels& p, double radius, double amount, int threshold) {
    if (radius <= 0 || amount <= 0 || p.w == 0) return;
    auto blurred = blurBuffer(p, radius);
    for (size_t i = 0; i < p.d.size(); i += 4)
        for (int c = 0; c < 3; ++c) {
            int diff = int(p.d[i + c]) - int(blurred[i + c]);
            if (std::abs(diff) < threshold) continue;
            p.d[i + c] = clamp8(p.d[i + c] + diff * amount);
        }
}

void median(Pixels& p, int radius) {
    if (radius <= 0 || p.w == 0) return;
    Pixels src = p;
    std::vector<uint8_t> window;
    for (int y = 0; y < p.h; ++y)
        for (int x = 0; x < p.w; ++x)
            for (int c = 0; c < 4; ++c) {
                window.clear();
                for (int dy = -radius; dy <= radius; ++dy)
                    for (int dx = -radius; dx <= radius; ++dx) {
                        int sx = std::clamp(x + dx, 0, p.w - 1), sy = std::clamp(y + dy, 0, p.h - 1);
                        window.push_back(src.px(sx, sy)[c]);
                    }
                auto mid = window.begin() + window.size() / 2;
                std::nth_element(window.begin(), mid, window.end());
                p.px(x, y)[c] = *mid;
            }
}

void addNoise(Pixels& p, double amount, bool monochrome, unsigned seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> dist(0.0, amount);
    for (size_t i = 0; i < p.d.size(); i += 4) {
        if (monochrome) {
            double n = dist(rng);
            for (int c = 0; c < 3; ++c) p.d[i + c] = clamp8(p.d[i + c] + n);
        } else {
            for (int c = 0; c < 3; ++c) p.d[i + c] = clamp8(p.d[i + c] + dist(rng));
        }
    }
}

}  // namespace Filters
