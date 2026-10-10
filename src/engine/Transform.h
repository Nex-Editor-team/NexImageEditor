#pragma once
#include "ImageBuffer.h"
#include <QTransform>
#include <QSizeF>
#include <QPointF>

enum class ResizeMode { Nearest, Bilinear };

namespace Transform {

// ---- Destructive / one-shot pixel operations ----
ImageBuffer resize(const ImageBuffer& src, int newWidth, int newHeight, ResizeMode mode = ResizeMode::Bilinear);
ImageBuffer crop(const ImageBuffer& src, const QRect& rect);
ImageBuffer rotate90(const ImageBuffer& src, bool clockwise);
ImageBuffer flipHorizontal(const ImageBuffer& src);
ImageBuffer flipVertical(const ImageBuffer& src);

// ---- Free Transform: live, non-destructive placement of a layer ----
//
// A layer's on-canvas geometry is fully described by this state. It is
// applied every frame via toQTransform() while the user drags handles
// (cheap, GPU/CPU-friendly via QPainter::setTransform), and only
// rasterized into new pixels ("baked") when the user explicitly commits
// via applyFreeTransform() (e.g. "Apply Transform" / flatten).
struct FreeTransformState {
    QPointF center{0, 0};   // where the content's center sits on the canvas
    double scaleX = 1.0;
    double scaleY = 1.0;
    double rotationDeg = 0.0;
    double shearXDeg = 0.0; // horizontal skew, for the "free" part of Free Transform

    // Builds the local(content) -> canvas matrix: shear, scale and rotate
    // about the content's own center, then translate that center to
    // `center` on the canvas.
    QTransform toQTransform(const QSizeF& contentSize) const;

    bool operator==(const FreeTransformState& o) const {
        return center == o.center && scaleX == o.scaleX && scaleY == o.scaleY &&
               rotationDeg == o.rotationDeg && shearXDeg == o.shearXDeg;
    }
    bool operator!=(const FreeTransformState& o) const { return !(*this == o); }
};

// Handles used to manipulate a FreeTransformState interactively on the canvas.
enum class TransformHandle {
    None,
    TopLeft, TopMiddle, TopRight,
    MiddleLeft, MiddleRight,
    BottomLeft, BottomMiddle, BottomRight,
    Rotate,
    Body // inside the box: move/translate
};

// Rasterizes `src` under `state` into a new buffer of `outputSize`
// (typically the canvas size). Used to permanently commit a transform.
ImageBuffer applyFreeTransform(const ImageBuffer& src, const FreeTransformState& state, const QSize& outputSize);

} // namespace Transform
