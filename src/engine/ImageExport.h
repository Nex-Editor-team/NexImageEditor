#pragma once
#include "ImageBuffer.h"

namespace ImageExport {
enum class Interpolation { Nearest, Bilinear, Bicubic, Lanczos, Area };
constexpr int MaxDimension = 20000;
constexpr qint64 MaxPixels = 100000000;
bool validSize(QSize size);
// Resize flattened pixels in premultiplied-alpha space to avoid dark fringes.
// Does not modify the source or the project. Null means invalid size/allocation failure.
ImageBuffer prepare(const ImageBuffer& source, QSize size, Interpolation interpolation,
                    bool transparent = true, QColor background = Qt::white);
}
