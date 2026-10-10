#pragma once
#include "core/Document.h"
#include "core/Pixels.h"
#include "core/Selection.h"

#include <QFont>
#include <QPainterPath>
#include <QPolygonF>
#include <QTransform>
#include <functional>

namespace Ops {

// Applies fn to the layer's pixels inside the selection (all pixels if sel is null),
// blending by selection coverage so feathered/antialiased edges work.
void applyPixelFn(Layer& layer, const Selection* sel, const std::function<void(Pixels&)>& fn);

// Canvas-space masks (w*h, 0..255).
std::vector<uint8_t> rectMask(const QRect& r, int w, int h);
std::vector<uint8_t> ellipseMask(const QRect& r, int w, int h);
std::vector<uint8_t> polygonMask(const QPolygonF& poly, int w, int h);

// Flood fill / magic wand on the given image (layer coords). Returns a mask in layer coords.
std::vector<uint8_t> floodMask(const QImage& img, QPoint seed, int tolerance, bool contiguous);

// Painting. Coordinates are canvas coordinates; they are clipped to the selection.
void strokeBrush(Layer& layer, const Selection* sel, const QPointF& a, const QPointF& b,
                 double size, const QColor& color, double hardness, bool erase);
// layerMask: layer-image sized coverage (0..255), see layerMaskFromSelection().
void fillWithMask(Layer& layer, const std::vector<uint8_t>& layerMask, const QColor& color);
void fillGradient(Layer& layer, const Selection* sel, const QPointF& from, const QPointF& to,
                  const QColor& c1, const QColor& c2);

// Converts a canvas-space selection into layer-image coordinates (nullptr sel = all opaque).
std::vector<uint8_t> layerMaskFromSelection(const Layer& layer, const Selection* sel);
void drawShape(Layer& layer, const Selection* sel, const QRectF& rect, bool ellipse,
               const QColor& fill, const QColor& stroke, double strokeW);
void drawText(Layer& layer, const Selection* sel, const QPointF& pos, const QString& text,
              const QFont& font, const QColor& color);

// Moves the selected pixels of a layer by (dx, dy) in place (cuts and pastes).
void moveSelectedPixels(Layer& layer, const Selection& sel, int dx, int dy);

// Transforms: layer gets a new image covering the transformed bounds.
void transformLayer(Layer& layer, const QTransform& t);
QRect transformedBounds(const QRect& r, const QTransform& t);

// Document-level.
void cropDocument(Document& doc, const QRect& r);
void resizeImage(Document& doc, int w, int h);
void resizeCanvas(Document& doc, int w, int h, int anchorX, int anchorY);  // anchor 0..2
void rotateDocument(Document& doc, int degrees);  // 90, -90, 180
void flipDocument(Document& doc, bool horizontal);

QImage cropImage(const QImage& img, const QRect& r);

}  // namespace Ops
