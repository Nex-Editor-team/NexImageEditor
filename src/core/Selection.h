#pragma once
#include <QPainterPath>
#include <QRect>
#include <cstdint>
#include <memory>
#include <vector>

enum class SelOp { Replace, Add, Subtract };

// A selection is a canvas-sized coverage mask (0..255) plus derived paths:
// `clip` (covered pixel runs, used for clipping painters) and `outline`
// (boundary edges, drawn as marching ants).
struct Selection {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> mask;
    QPainterPath clip;
    QPainterPath outline;

    uint8_t at(int x, int y) const {
        return (x < 0 || y < 0 || x >= w || y >= h) ? 0 : mask[size_t(y) * size_t(w) + size_t(x)];
    }
    QRect bounds() const;

    // Returns nullptr when the mask is empty.
    static std::shared_ptr<Selection> fromMask(std::vector<uint8_t> mask, int w, int h);
    static std::shared_ptr<Selection> fromPath(const QPainterPath& path, int w, int h);
};

using SelectionPtr = std::shared_ptr<Selection>;

// Combines an existing selection with a new one (may return nullptr = nothing selected).
SelectionPtr combineSelections(const SelectionPtr& base, const SelectionPtr& add, SelOp op,
                               int w, int h);
