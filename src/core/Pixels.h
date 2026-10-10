#pragma once
#include "core/Surface.h"

#include <cstdint>
#include <vector>

// Non-premultiplied RGBA8 buffer used by the per-pixel algorithms.
struct Pixels {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> d;  // R G B A per pixel

    uint8_t* px(int x, int y) { return &d[(size_t(y) * size_t(w) + size_t(x)) * 4]; }
    const uint8_t* px(int x, int y) const { return &d[(size_t(y) * size_t(w) + size_t(x)) * 4]; }
};

Pixels toPixels(const Surface& s);
Surface fromPixels(const Pixels& p);
