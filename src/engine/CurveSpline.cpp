#include "CurveSpline.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace CurveSpline {

std::array<int, 256> buildLut(const QVector<QPointF>& inputPoints) {
    std::array<int, 256> lut{};
    for (int i = 0; i < 256; ++i) lut[i] = i; // Identity fallback

    if (inputPoints.isEmpty()) {
        return lut;
    }

    // Copy and sort points by X coordinate
    std::vector<QPointF> pts;
    pts.reserve(inputPoints.size());
    for (const auto& p : inputPoints) {
        pts.push_back(p);
    }
    std::sort(pts.begin(), pts.end(), [](const QPointF& a, const QPointF& b) {
        return a.x() < b.x();
    });

    // Remove duplicates or points with near-identical X
    std::vector<QPointF> cleanPts;
    cleanPts.reserve(pts.size());
    for (const auto& p : pts) {
        if (cleanPts.empty() || std::abs(cleanPts.back().x() - p.x()) > 0.01) {
            cleanPts.push_back(p);
        } else {
            cleanPts.back().setY(p.y()); // overwrite Y
        }
    }

    if (cleanPts.empty()) return lut;

    if (cleanPts.size() == 1) {
        int val = std::clamp(static_cast<int>(std::round(cleanPts[0].y())), 0, 255);
        lut.fill(val);
        return lut;
    }

    // Ensure left and right endpoints exist
    if (cleanPts.front().x() > 0.0) {
        cleanPts.insert(cleanPts.begin(), QPointF(0.0, cleanPts.front().y()));
    }
    if (cleanPts.back().x() < 255.0) {
        cleanPts.push_back(QPointF(255.0, cleanPts.back().y()));
    }

    size_t n = cleanPts.size();
    std::vector<double> x(n), y(n);
    for (size_t i = 0; i < n; ++i) {
        x[i] = cleanPts[i].x();
        y[i] = cleanPts[i].y();
    }

    // 1. Secant slopes
    std::vector<double> delta(n - 1);
    for (size_t i = 0; i < n - 1; ++i) {
        double dx = x[i + 1] - x[i];
        delta[i] = (dx > 1e-6) ? (y[i + 1] - y[i]) / dx : 0.0;
    }

    // 2. Initial tangents
    std::vector<double> m(n, 0.0);
    m[0] = delta[0];
    m[n - 1] = delta[n - 2];
    for (size_t i = 1; i < n - 1; ++i) {
        m[i] = 0.5 * (delta[i - 1] + delta[i]);
    }

    // 3. Fritsch-Carlson monotonicity condition
    for (size_t i = 0; i < n - 1; ++i) {
        if (std::abs(delta[i]) < 1e-9) {
            m[i] = 0.0;
            m[i + 1] = 0.0;
        } else {
            double alpha = m[i] / delta[i];
            double beta = m[i + 1] / delta[i];
            double sumSq = alpha * alpha + beta * beta;
            if (sumSq > 9.0) {
                double tau = 3.0 / std::sqrt(sumSq);
                m[i] = tau * alpha * delta[i];
                m[i + 1] = tau * beta * delta[i];
            }
        }
    }

    // 4. Evaluate Hermite cubic spline for all integer values in 0..255
    size_t segment = 0;
    for (int i = 0; i < 256; ++i) {
        double curX = static_cast<double>(i);

        if (curX <= x[0]) {
            lut[i] = std::clamp(static_cast<int>(std::round(y[0])), 0, 255);
            continue;
        }
        if (curX >= x[n - 1]) {
            lut[i] = std::clamp(static_cast<int>(std::round(y[n - 1])), 0, 255);
            continue;
        }

        while (segment < n - 2 && curX > x[segment + 1]) {
            segment++;
        }

        double h = x[segment + 1] - x[segment];
        if (h <= 1e-6) {
            lut[i] = std::clamp(static_cast<int>(std::round(y[segment])), 0, 255);
            continue;
        }

        double t = (curX - x[segment]) / h;
        double t2 = t * t;
        double t3 = t2 * t;

        double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
        double h10 = t3 - 2.0 * t2 + t;
        double h01 = -2.0 * t3 + 3.0 * t2;
        double h11 = t3 - t2;

        double val = h00 * y[segment] + h10 * h * m[segment] + h01 * y[segment + 1] + h11 * h * m[segment + 1];
        lut[i] = std::clamp(static_cast<int>(std::round(val)), 0, 255);
    }

    return lut;
}

} // namespace CurveSpline
