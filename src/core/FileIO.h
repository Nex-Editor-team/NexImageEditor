#pragma once
#include "core/Document.h"

#include <string>

namespace fileio {
// Raster import via gdk-pixbuf (PNG, JPEG, WEBP, BMP, TIFF, ...). Returns a null surface on failure.
Surface importImage(const std::string& path, std::string& error);

// Raster export. Format is chosen from the extension: png, jpg/jpeg, webp, bmp, tif/tiff.
// JPEG has no alpha, so transparent pixels are flattened onto white.
bool exportImage(const std::string& path, const Surface& flat, std::string& error);

// Native project format (.nxe): magic "NXE1", canvas size, then one PNG-compressed block per layer.
bool saveProject(const std::string& path, const Document& doc, std::string& error);
bool loadProject(const std::string& path, Document& doc, std::string& error);

bool isProjectPath(const std::string& path);
}
