#pragma once
#include "../engine/ImageBuffer.h"
#include <QString>

// Thin wrapper over QImageReader/Writer. PNG and JPEG work with a stock
// Qt install; WebP additionally requires the qtimageformats plugin
// (see CMakeLists.txt note) - loadImage()/saveImage() simply fail
// gracefully (return false / null buffer) if the plugin isn't present.
namespace ImageIO {
    ImageBuffer loadImage(const QString& path);

    // quality: -1 = format default, 0..100 for lossy formats (JPEG/WebP).
    bool saveImage(const QString& path, const ImageBuffer& buffer, int quality = -1);

    bool isSupportedExtension(const QString& path);
}
