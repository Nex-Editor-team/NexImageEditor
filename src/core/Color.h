#pragma once
#include <cstdint>

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

inline Color rgba(int r, int g, int b, int a = 255) {
    return Color{uint8_t(r), uint8_t(g), uint8_t(b), uint8_t(a)};
}
