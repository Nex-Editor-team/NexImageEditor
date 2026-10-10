#pragma once
#include "ImageBuffer.h"
#include "ToneParameters.h"
#include <array>

struct HistogramData {
    std::array<int, 256> r{};
    std::array<int, 256> g{};
    std::array<int, 256> b{};
    std::array<int, 256> lum{};
    int maxCount = 1;

    void clear() {
        r.fill(0);
        g.fill(0);
        b.fill(0);
        lum.fill(0);
        maxCount = 1;
    }
};

namespace PhotoEngine {
    // Computes full multi-channel histogram data from an image buffer
    HistogramData computeHistogram(const ImageBuffer& src);

    // Automatically computes optimal tone parameters based on histogram
    ToneParameters computeAutoTone(const ImageBuffer& src);

    // Applies full tone adjustments, curves, color grading, and effects to image
    ImageBuffer apply(const ImageBuffer& src, const ToneParameters& params);
}
