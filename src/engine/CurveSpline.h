#pragma once
#include <QVector>
#include <QPointF>
#include <array>

namespace CurveSpline {
    // Generates a 256-entry lookup table (LUT) from input control points.
    // Points must be in [0..255] coordinate space.
    // Handles sorting, deduplication, and monotone cubic spline interpolation.
    std::array<int, 256> buildLut(const QVector<QPointF>& points);
}
