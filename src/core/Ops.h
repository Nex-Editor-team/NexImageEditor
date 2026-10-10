#pragma once
#include "core/Document.h"

#include <cairo.h>

#include <vector>

// Selection builders. Masks are canvas-sized.
namespace ops {
SelectionPtr selRect(IRect r, int w, int h);
SelectionPtr selEllipse(IRect r, int w, int h);
SelectionPtr selPolygon(const std::vector<IPoint>& pts, int w, int h);
SelectionPtr selMagicWand(const Surface& flat, IPoint seed, int tolerance, bool contiguous);

// Pixel edits on a layer. `sel` (canvas-sized, may be null) clips every edit.
void brushStroke(Layer& L, const Selection* sel, PointD a, PointD b, double size, Color c, bool erase);
void bucketFill(Layer& L, const Selection* sel, IPoint canvasSeed, Color c, int tolerance, bool contiguous);
void gradientFill(Layer& L, const Selection* sel, PointD a, PointD b, Color c1, Color c2);
void fillSelection(Layer& L, const Selection* sel, Color c);
void clearSelection(Layer& L, const Selection* sel);
void drawShape(Layer& L, const Selection* sel, IRect r, bool ellipse, bool fill, Color fillColor,
               bool stroke, Color strokeColor, double strokeWidth);
void drawText(Layer& L, const Selection* sel, IPoint at, const std::string& text, const std::string& family,
              int sizePx, Color c);
void moveSelectedPixels(Layer& L, const Selection& sel, int dx, int dy);

// Applies a canvas-space affine matrix to one layer. The layer's image is replaced by its bounding box.
void transformLayer(Layer& L, const cairo_matrix_t& canvasMatrix);

// Canvas-level operations (all layers).
void cropDocument(Document& d, IRect r);
void resizeCanvas(Document& d, int w, int h, int offsetX, int offsetY);
void resizeImage(Document& d, int w, int h);
void rotateCanvas90(Document& d, bool clockwise);
void rotateCanvas180(Document& d);
void flipCanvas(Document& d, bool horizontal);

// Blends `result` (layer-sized) into the layer, keeping pixels outside the selection.
void applyResult(Layer& L, const Surface& result, const Selection* sel);
}
