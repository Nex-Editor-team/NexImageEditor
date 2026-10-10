#pragma once
#include "ImageBuffer.h"

// High-quality sharpening backed by OpenCV, as an alternative to the
// dependency-free box-blur-based Filters::sharpen(). OpenCV's Gaussian
// blur is a true separable Gaussian (not a box approximation), which
// gives a noticeably cleaner unsharp mask with less "halo" artifacting,
// plus a couple of extra modes useful for photos.
namespace CvSharpen {

// Classic unsharp mask: result = src + amount * (src - GaussianBlur(src, radius)).
// - amount: 0..3.0 (0 = no change, ~1.0 = typical, higher = more aggressive)
// - radiusPx: Gaussian sigma in pixels (bigger = broader halo, coarser detail)
// - threshold: 0..255, minimum brightness difference before sharpening kicks in
//   (protects flat areas/noise from being sharpened; 0 = sharpen everything)
ImageBuffer unsharpMask(const ImageBuffer& src, double amount, double radiusPx, int threshold = 0);

// "Smart"/detail sharpening: edge-aware smoothing (bilateral-like) before
// the unsharp pass, so noise isn't amplified along with real detail.
// strength: 0..1 (how much edge-preserving smoothing to apply first)
ImageBuffer smartSharpen(const ImageBuffer& src, double amount, double radiusPx, double strength);

} // namespace CvSharpen
