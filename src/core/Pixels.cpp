#include "core/Pixels.h"

#include <algorithm>

Pixels toPixels(const Surface& s) {
    surfFlush(s);
    Pixels p;
    p.w = surfW(s);
    p.h = surfH(s);
    p.d.resize(size_t(p.w) * size_t(p.h) * 4);
    for (int y = 0; y < p.h; ++y) {
        const uint32_t* row = surfRow(s, y);
        for (int x = 0; x < p.w; ++x) {
            uint32_t v = row[x];
            uint32_t a = v >> 24;
            uint8_t* q = p.px(x, y);
            if (a == 0) {
                q[0] = q[1] = q[2] = q[3] = 0;
                continue;
            }
            for (int c = 0; c < 3; ++c) {
                uint32_t pre = (v >> (16 - 8 * c)) & 0xff;
                q[c] = uint8_t(std::min<uint32_t>(255, (pre * 255 + a / 2) / a));
            }
            q[3] = uint8_t(a);
        }
    }
    return p;
}

Surface fromPixels(const Pixels& p) {
    Surface s = newSurface(p.w, p.h);
    for (int y = 0; y < p.h; ++y) {
        uint32_t* row = surfRow(s, y);
        for (int x = 0; x < p.w; ++x) {
            const uint8_t* q = p.px(x, y);
            uint32_t a = q[3];
            uint32_t r = (q[0] * a + 127) / 255, g = (q[1] * a + 127) / 255, b = (q[2] * a + 127) / 255;
            row[x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    surfMarkDirty(s);
    return s;
}
