#pragma once
#include <QVector>
#include <QPointF>
#include <QString>
#include <cmath>

struct ToneParameters {
    // ---- Light (Ánh sáng) ----
    double exposure = 0.0;     // -5.00 .. +5.00 EV (or -100 .. +100 mapped)
    double contrast = 0.0;     // -100 .. +100
    double highlights = 0.0;   // -100 .. +100
    double shadows = 0.0;      // -100 .. +100
    double whites = 0.0;       // -100 .. +100
    double blacks = 0.0;       // -100 .. +100

    // ---- Color (Màu sắc) ----
    double temp = 0.0;         // -100 (Cool) .. +100 (Warm)
    double tint = 0.0;         // -100 (Green) .. +100 (Magenta)
    double vibrance = 0.0;     // -100 .. +100
    double saturation = 0.0;   // -100 .. +100

    // ---- Effects & Details (Chi tiết & Hiệu ứng) ----
    double clarity = 0.0;      // -100 .. +100
    double dehaze = 0.0;       // -100 .. +100
    double vignette = 0.0;     // -100 .. +100

    // ---- Quick Modes ----
    bool bwMode = false;       // Black & White conversion
    bool hdrMode = false;      // HDR tone mapping

    // ---- Presets ----
    QString activePreset = "Mặc định";
    double presetAmount = 100.0; // 0% .. 200%

    // ---- Point Curves (Đường cong điểm: 0..255 space) ----
    QVector<QPointF> curveRgb{{0.0, 0.0}, {255.0, 255.0}};
    QVector<QPointF> curveRed{{0.0, 0.0}, {255.0, 255.0}};
    QVector<QPointF> curveGreen{{0.0, 0.0}, {255.0, 255.0}};
    QVector<QPointF> curveBlue{{0.0, 0.0}, {255.0, 255.0}};

    static bool isCurveDefault(const QVector<QPointF>& c) {
        if (c.size() != 2) return false;
        return std::abs(c[0].x()) < 1e-3 && std::abs(c[0].y()) < 1e-3 &&
               std::abs(c[1].x() - 255.0) < 1e-3 && std::abs(c[1].y() - 255.0) < 1e-3;
    }

    bool isIdentity() const {
        return std::abs(exposure) < 1e-3 &&
               std::abs(contrast) < 1e-3 &&
               std::abs(highlights) < 1e-3 &&
               std::abs(shadows) < 1e-3 &&
               std::abs(whites) < 1e-3 &&
               std::abs(blacks) < 1e-3 &&
               std::abs(temp) < 1e-3 &&
               std::abs(tint) < 1e-3 &&
               std::abs(vibrance) < 1e-3 &&
               std::abs(saturation) < 1e-3 &&
               std::abs(clarity) < 1e-3 &&
               std::abs(dehaze) < 1e-3 &&
               std::abs(vignette) < 1e-3 &&
               !bwMode && !hdrMode &&
               isCurveDefault(curveRgb) &&
               isCurveDefault(curveRed) &&
               isCurveDefault(curveGreen) &&
               isCurveDefault(curveBlue);
    }

    void reset() {
        exposure = 0.0;
        contrast = 0.0;
        highlights = 0.0;
        shadows = 0.0;
        whites = 0.0;
        blacks = 0.0;

        temp = 0.0;
        tint = 0.0;
        vibrance = 0.0;
        saturation = 0.0;

        clarity = 0.0;
        dehaze = 0.0;
        vignette = 0.0;

        bwMode = false;
        hdrMode = false;

        activePreset = "Mặc định";
        presetAmount = 100.0;

        resetCurves();
    }

    void resetLight() {
        exposure = 0.0;
        contrast = 0.0;
        highlights = 0.0;
        shadows = 0.0;
        whites = 0.0;
        blacks = 0.0;
    }

    void resetColor() {
        temp = 0.0;
        tint = 0.0;
        vibrance = 0.0;
        saturation = 0.0;
        bwMode = false;
    }

    void resetEffects() {
        clarity = 0.0;
        dehaze = 0.0;
        vignette = 0.0;
        hdrMode = false;
    }

    void resetCurves() {
        curveRgb = {{0.0, 0.0}, {255.0, 255.0}};
        curveRed = {{0.0, 0.0}, {255.0, 255.0}};
        curveGreen = {{0.0, 0.0}, {255.0, 255.0}};
        curveBlue = {{0.0, 0.0}, {255.0, 255.0}};
    }

    bool operator==(const ToneParameters& o) const {
        return std::abs(exposure - o.exposure) < 1e-3 &&
               std::abs(contrast - o.contrast) < 1e-3 &&
               std::abs(highlights - o.highlights) < 1e-3 &&
               std::abs(shadows - o.shadows) < 1e-3 &&
               std::abs(whites - o.whites) < 1e-3 &&
               std::abs(blacks - o.blacks) < 1e-3 &&
               std::abs(temp - o.temp) < 1e-3 &&
               std::abs(tint - o.tint) < 1e-3 &&
               std::abs(vibrance - o.vibrance) < 1e-3 &&
               std::abs(saturation - o.saturation) < 1e-3 &&
               std::abs(clarity - o.clarity) < 1e-3 &&
               std::abs(dehaze - o.dehaze) < 1e-3 &&
               std::abs(vignette - o.vignette) < 1e-3 &&
               bwMode == o.bwMode &&
               hdrMode == o.hdrMode &&
               activePreset == o.activePreset &&
               std::abs(presetAmount - o.presetAmount) < 1e-3 &&
               curveRgb == o.curveRgb &&
               curveRed == o.curveRed &&
               curveGreen == o.curveGreen &&
               curveBlue == o.curveBlue;
    }

    bool operator!=(const ToneParameters& o) const {
        return !(*this == o);
    }
};
