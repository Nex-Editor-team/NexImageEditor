#pragma once
#include <algorithm>

struct IPoint {
    int x = 0, y = 0;
};

struct PointD {
    double x = 0, y = 0;
};

struct IRect {
    int x = 0, y = 0, w = 0, h = 0;
    bool empty() const { return w <= 0 || h <= 0; }
    int right() const { return x + w; }
    int bottom() const { return y + h; }
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    IRect intersected(const IRect& o) const {
        int x0 = std::max(x, o.x), y0 = std::max(y, o.y);
        int x1 = std::min(right(), o.right()), y1 = std::min(bottom(), o.bottom());
        if (x1 <= x0 || y1 <= y0) return IRect{};
        return IRect{x0, y0, x1 - x0, y1 - y0};
    }
};
