#include "PhotoEngine.h"
#include "CurveSpline.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

inline int clamp255(int v) {
    return (v < 0) ? 0 : ((v > 255) ? 255 : v);
}

inline double clamp01(double v) {
    return (v < 0.0) ? 0.0 : ((v > 1.0) ? 1.0 : v);
}

inline double smoothstep(double edge0, double edge1, double x) {
    double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

} // namespace

namespace PhotoEngine {

HistogramData computeHistogram(const ImageBuffer& src) {
    HistogramData data;
    if (src.isNull()) return data;

    const QImage& img = src.image();
    int w = img.width();
    int h = img.height();
    if (w <= 0 || h <= 0) return data;

    QImage argb = (img.format() == QImage::Format_ARGB32 || img.format() == QImage::Format_RGB32)
                      ? img
                      : img.convertToFormat(QImage::Format_ARGB32);

    int maxVal = 1;
    for (int y = 0; y < h; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            QRgb px = line[x];
            int r = qRed(px);
            int g = qGreen(px);
            int b = qBlue(px);
            int lum = clamp255(static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b + 0.5));

            data.r[r]++;
            data.g[g]++;
            data.b[b]++;
            data.lum[lum]++;

            if (data.r[r] > maxVal) maxVal = data.r[r];
            if (data.g[g] > maxVal) maxVal = data.g[g];
            if (data.b[b] > maxVal) maxVal = data.b[b];
            if (data.lum[lum] > maxVal) maxVal = data.lum[lum];
        }
    }
    data.maxCount = maxVal;
    return data;
}

ToneParameters computeAutoTone(const ImageBuffer& src) {
    ToneParameters res;
    if (src.isNull()) return res;

    HistogramData hist = computeHistogram(src);
    int totalPixels = src.width() * src.height();
    if (totalPixels <= 0) return res;

    // Find 1st and 99th percentile of luminance
    int count = 0;
    int p1 = 0;
    int p99 = 255;
    int median = 128;
    double sumLum = 0.0;

    int p1Threshold = totalPixels * 0.01;
    int p50Threshold = totalPixels * 0.50;
    int p99Threshold = totalPixels * 0.99;

    for (int i = 0; i < 256; ++i) {
        count += hist.lum[i];
        sumLum += i * hist.lum[i];
        if (p1 == 0 && count >= p1Threshold) p1 = i;
        if (median == 128 && count >= p50Threshold) median = i;
        if (p99 == 255 && count >= p99Threshold) p99 = i;
    }

    double meanLum = sumLum / totalPixels;

    // 1. Exposure adjustment towards target middle luminance (~125)
    double targetLum = 125.0;
    double expShift = (targetLum - meanLum) / 50.0;
    res.exposure = std::clamp(expShift, -2.5, 2.5);

    // 2. Contrast adjustment: boost contrast if dynamic range is narrow
    int dynamicRange = p99 - p1;
    if (dynamicRange < 140) {
        res.contrast = std::clamp((170 - dynamicRange) * 0.6, 10.0, 45.0);
    } else if (dynamicRange > 240) {
        res.contrast = -10.0;
    } else {
        res.contrast = 15.0;
    }

    // 3. Highlights: compress if highlights are clipped
    if (p99 > 240) {
        res.highlights = std::clamp(-static_cast<double>(p99 - 220) * 2.0, -80.0, -15.0);
        res.whites = -15.0;
    } else {
        res.highlights = -10.0;
        res.whites = 10.0;
    }

    // 4. Shadows: lift if shadows are clipped
    if (p1 < 25) {
        res.shadows = std::clamp(static_cast<double>(35 - p1) * 2.0, 15.0, 70.0);
        res.blacks = 10.0;
    } else {
        res.shadows = 20.0;
        res.blacks = -10.0;
    }

    res.vibrance = 15.0; // gentle natural boost
    res.saturation = 5.0;

    return res;
}

ImageBuffer apply(const ImageBuffer& src, const ToneParameters& params) {
    if (src.isNull()) return {};
    if (params.isIdentity()) return src;

    QImage img = src.image();
    int w = img.width();
    int h = img.height();
    if (w <= 0 || h <= 0) return src;

    QImage dst = img.convertToFormat(QImage::Format_ARGB32);

    // 1. Compute spline lookup tables for curves
    std::array<int, 256> curveLutRgb = CurveSpline::buildLut(params.curveRgb);
    std::array<int, 256> curveLutRed = CurveSpline::buildLut(params.curveRed);
    std::array<int, 256> curveLutGreen = CurveSpline::buildLut(params.curveGreen);
    std::array<int, 256> curveLutBlue = CurveSpline::buildLut(params.curveBlue);

    // 2. Build combined Light & Tone LUTs for R, G, B channels
    double expScale = std::pow(2.0, params.exposure);
    double contrastVal = params.contrast / 100.0;
    double contrastFactor = (contrastVal >= 0.0) ? (1.0 + contrastVal * 1.5) : (1.0 + contrastVal * 0.7);

    double highlightsVal = params.highlights / 100.0; // -1 .. +1
    double shadowsVal = params.shadows / 100.0;       // -1 .. +1
    double whitesVal = params.whites / 100.0;         // -1 .. +1
    double blacksVal = params.blacks / 100.0;         // -1 .. +1

    // Temperature & Tint multipliers
    double tempVal = params.temp / 100.0; // -1 .. +1 (Cool .. Warm)
    double tintVal = params.tint / 100.0; // -1 .. +1 (Green .. Magenta)

    double rBalance = 1.0 + (tempVal * 0.25) + (tintVal * 0.15);
    double gBalance = 1.0 - (tintVal * 0.20);
    double bBalance = 1.0 - (tempVal * 0.25) + (tintVal * 0.10);

    // Fast Tone Mapping Function for normalized input in [0, 1]
    auto toneMap = [&](double v) -> double {
        // 1. Exposure
        v = v * expScale;

        // 2. Highlights & Shadows adjustments
        if (shadowsVal != 0.0) {
            double shadowWeight = 1.0 - smoothstep(0.0, 0.65, v);
            v += shadowsVal * 0.35 * shadowWeight * (1.0 - v);
        }
        if (highlightsVal != 0.0) {
            double highlightWeight = smoothstep(0.35, 1.0, v);
            if (highlightsVal < 0.0) {
                // Highlight recovery: compress top end
                v += highlightsVal * 0.40 * highlightWeight * v;
            } else {
                v += highlightsVal * 0.30 * highlightWeight * (1.0 - v);
            }
        }

        // 3. Whites & Blacks anchors
        if (blacksVal != 0.0) {
            double blackWeight = 1.0 - smoothstep(0.0, 0.30, v);
            v += blacksVal * 0.25 * blackWeight;
        }
        if (whitesVal != 0.0) {
            double whiteWeight = smoothstep(0.70, 1.0, v);
            v += whitesVal * 0.25 * whiteWeight;
        }

        // 4. Contrast S-curve centered around 0.5
        v = clamp01(v);
        v = 0.5 + (v - 0.5) * contrastFactor;

        // 5. HDR local tone expansion/compression simulation
        if (params.hdrMode) {
            // S-curve tone boost with lifted darks and tamed brights
            v = std::sin(v * 1.5707963); // smooth sine curve
        }

        return clamp01(v);
    };

    // Precalculate 256-entry channel LUTs
    std::array<int, 256> lutR{}, lutG{}, lutB{};
    for (int i = 0; i < 256; ++i) {
        // Master curve
        int cRgb = curveLutRgb[i];

        // Channel specific curves
        int cR = curveLutRed[cRgb];
        int cG = curveLutGreen[cRgb];
        int cB = curveLutBlue[cRgb];

        double rNorm = (cR / 255.0) * rBalance;
        double gNorm = (cG / 255.0) * gBalance;
        double bNorm = (cB / 255.0) * bBalance;

        lutR[i] = clamp255(static_cast<int>(toneMap(rNorm) * 255.0 + 0.5));
        lutG[i] = clamp255(static_cast<int>(toneMap(gNorm) * 255.0 + 0.5));
        lutB[i] = clamp255(static_cast<int>(toneMap(bNorm) * 255.0 + 0.5));
    }

    // Color controls
    double satFactor = 1.0 + (params.saturation / 100.0);
    double vibVal = params.vibrance / 100.0;
    bool hasColorAdjust = (std::abs(params.saturation) > 1e-3 || std::abs(params.vibrance) > 1e-3 || params.bwMode);

    // Vignette precalculation
    bool hasVignette = std::abs(params.vignette) > 1e-3;
    double cx = w / 2.0;
    double cy = h / 2.0;
    double maxDist = std::sqrt(cx * cx + cy * cy);
    double vignetteAmt = params.vignette / 100.0;

    // Apply LUTs and pixel operations
    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(dst.scanLine(y));
        double dy = (y - cy);

        for (int x = 0; x < w; ++x) {
            QRgb px = line[x];
            int a = qAlpha(px);
            if (a == 0) continue;

            int r = lutR[qRed(px)];
            int g = lutG[qGreen(px)];
            int b = lutB[qBlue(px)];

            if (params.bwMode) {
                int gray = clamp255(static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b + 0.5));
                r = g = b = gray;
            } else if (hasColorAdjust) {
                int maxC = std::max({r, g, b});
                int minC = std::min({r, g, b});
                double curSat = (maxC > 0) ? static_cast<double>(maxC - minC) / maxC : 0.0;

                // Vibrance boosts less saturated colors more, protecting skin tones
                double vibFactor = 1.0 + vibVal * (1.0 - curSat) * 1.5;
                double totalSat = std::max(0.0, satFactor * vibFactor);

                double gray = 0.299 * r + 0.587 * g + 0.114 * b;
                r = clamp255(static_cast<int>(gray + (r - gray) * totalSat + 0.5));
                g = clamp255(static_cast<int>(gray + (g - gray) * totalSat + 0.5));
                b = clamp255(static_cast<int>(gray + (b - gray) * totalSat + 0.5));
            }

            // Vignette shading
            if (hasVignette) {
                double dx = (x - cx);
                double distNorm = std::sqrt(dx * dx + dy * dy) / maxDist;
                double vFactor = 1.0 - vignetteAmt * smoothstep(0.35, 1.15, distNorm);
                vFactor = std::clamp(vFactor, 0.0, 2.0);
                r = clamp255(static_cast<int>(r * vFactor + 0.5));
                g = clamp255(static_cast<int>(g * vFactor + 0.5));
                b = clamp255(static_cast<int>(b * vFactor + 0.5));
            }

            line[x] = qRgba(r, g, b, a);
        }
    }

    return ImageBuffer(dst);
}

} // namespace PhotoEngine
