#pragma once
#include "core/Geometry.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

enum class SelOp { Replace, Add, Subtract };

// Canvas-sized coverage mask (0..255) plus boundary edges for the marching ants.
struct Selection {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> mask;
    std::vector<std::pair<IPoint, IPoint>> edges;

    uint8_t at(int x, int y) const {
        return (x < 0 || y < 0 || x >= w || y >= h) ? 0 : mask[size_t(y) * size_t(w) + size_t(x)];
    }
    IRect bounds() const;

    // Returns nullptr when the mask selects nothing.
    static std::shared_ptr<Selection> fromMask(std::vector<uint8_t> mask, int w, int h);
};

using SelectionPtr = std::shared_ptr<Selection>;

SelectionPtr combineSelections(const SelectionPtr& base, const SelectionPtr& add, SelOp op, int w, int h);
