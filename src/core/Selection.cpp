#include "core/Selection.h"

#include <algorithm>

IRect Selection::bounds() const {
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (mask[size_t(y) * size_t(w) + size_t(x)] > 127) {
                x0 = std::min(x0, x);
                y0 = std::min(y0, y);
                x1 = std::max(x1, x);
                y1 = std::max(y1, y);
            }
    if (x1 < 0) return IRect{};
    return IRect{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
}

std::shared_ptr<Selection> Selection::fromMask(std::vector<uint8_t> m, int w, int h) {
    auto s = std::make_shared<Selection>();
    s->w = w;
    s->h = h;
    s->mask = std::move(m);
    bool any = false;
    for (int y = 0; y < h && !any; ++y)
        for (int x = 0; x < w; ++x)
            if (s->at(x, y) > 127) {
                any = true;
                break;
            }
    if (!any) return nullptr;
    auto covered = [&](int x, int y) { return s->at(x, y) > 127; };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (!covered(x, y)) continue;
            if (!covered(x, y - 1)) s->edges.push_back({{x, y}, {x + 1, y}});
            if (!covered(x, y + 1)) s->edges.push_back({{x, y + 1}, {x + 1, y + 1}});
            if (!covered(x - 1, y)) s->edges.push_back({{x, y}, {x, y + 1}});
            if (!covered(x + 1, y)) s->edges.push_back({{x + 1, y}, {x + 1, y + 1}});
        }
    return s;
}

SelectionPtr combineSelections(const SelectionPtr& base, const SelectionPtr& add, SelOp op, int w, int h) {
    if (op == SelOp::Replace) return add;
    if (!base) return op == SelOp::Add ? add : nullptr;
    if (!add) return base;
    std::vector<uint8_t> m(size_t(w) * size_t(h));
    for (size_t i = 0; i < m.size(); ++i) {
        uint8_t a = base->mask[i];
        uint8_t b = add->mask[i];
        m[i] = op == SelOp::Add ? std::max(a, b) : std::min(a, uint8_t(255 - b));
    }
    return Selection::fromMask(std::move(m), w, h);
}
