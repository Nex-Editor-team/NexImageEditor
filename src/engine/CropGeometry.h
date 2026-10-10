#pragma once
#include <QPoint>
#include <QRect>
#include <QSize>

namespace CropGeometry {
// Coordinates are pixel boundaries (0..width/height), not pixel indices.
// A nonempty aspect uses integer multiples so ratios remain exact in pixels.
QRect fromDrag(QPoint anchor, QPoint end, QSize bounds, QSize aspect = {});
}
