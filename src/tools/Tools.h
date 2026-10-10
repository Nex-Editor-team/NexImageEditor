#pragma once
#include "tools/Tool.h"

#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>
#include <QTransform>

// Drags a rectangle or ellipse selection, or a freehand lasso.
class MarqueeTool : public Tool {
public:
    MarqueeTool(ToolHost& h, ToolId id);
    ToolId id() const override { return m_id; }
    QString displayName() const override;
    QString hint() const override;
    void mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) override;
    void mouseMove(const QPointF& p, Qt::KeyboardModifiers mods) override;
    void mouseRelease(const QPointF& p, Qt::KeyboardModifiers mods) override;
    void drawOverlay(QPainter& p, double zoom) override;

private:
    ToolId m_id;
    QPointF m_start, m_cur;
    QPolygonF m_lasso;
    bool m_dragging = false;
    SelOp opFromMods(Qt::KeyboardModifiers mods) const;
};

// Selects contiguous (or global) pixels of similar colour on the active layer.
class MagicWandTool : public Tool {
public:
    using Tool::Tool;
    ToolId id() const override { return ToolId::MagicWand; }
    QString displayName() const override { return "Magic Wand"; }
    QString hint() const override { return "Click to select similar colours. Shift adds, Alt subtracts."; }
    void mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) override;
};

// Moves the active layer, or the selected pixels when a selection exists.
class MoveTool : public Tool {
public:
    using Tool::Tool;
    ToolId id() const override { return ToolId::Move; }
    QString displayName() const override { return "Move"; }
    QString hint() const override { return "Drag to move the layer or the selected pixels."; }
    void mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) override;
    void mouseMove(const QPointF& p, Qt::KeyboardModifiers mods) override;
    void mouseRelease(const QPointF& p, Qt::KeyboardModifiers mods) override;
    void drawOverlay(QPainter& p, double zoom) override;
    bool hidesActiveLayer() const override { return m_selMode; }
    void cancel() override;

private:
    QPointF m_start, m_offset;
    bool m_dragging = false;
    bool m_selMode = false;
    QImage m_cut;       // layer with the selected pixels removed
    QImage m_floating;  // selected pixels only, layer-sized
    int m_layerX = 0, m_layerY = 0;
};

// Crop rectangle: drag, then press Enter to apply or Escape to cancel.
class CropTool : public Tool {
public:
    using Tool::Tool;
    ToolId id() const override { return ToolId::Crop; }
    QString displayName() const override { return "Crop"; }
    QString hint() const override { return "Drag to set the crop. Enter applies, Esc cancels."; }
    void mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) override;
    void mouseMove(const QPointF& p, Qt::KeyboardModifiers mods) override;
    void mouseRelease(const QPointF& p, Qt::KeyboardModifiers mods) override;
    bool keyPress(int key) override;
    void drawOverlay(QPainter& p, double zoom) override;
    void cancel() override { m_rect = QRectF(); m_dragging = false; }

private:
    QRectF m_rect;
    QPointF m_start;
    bool m_dragging = false;
};

// Free transform of the active layer: drag to move, corner handles to scale,
// the handle above the box to rotate. Enter commits, Esc cancels.
class TransformTool : public Tool {
public:
    using Tool::Tool;
    ToolId id() const override { return ToolId::Transform; }
    QString displayName() const override { return "Free Transform"; }
    QString hint() const override { return "Drag inside to move, corners to scale, top handle to rotate. Enter applies."; }
    void begin() override;
    void cancel() override;
    void mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) override;
    void mouseMove(const QPointF& p, Qt::KeyboardModifiers mods) override;
    void mouseRelease(const QPointF& p, Qt::KeyboardModifiers mods) override;
    bool keyPress(int key) override;
    void drawOverlay(QPainter& p, double zoom) override;
    bool hidesActiveLayer() const override { return m_active; }

private:
    enum class Drag { None, Move, Rotate, Corner };
    void commit();
    QPointF handlePos(int idx) const;  // 0..3 corners, 4 rotation handle

    bool m_active = false;
    QImage m_source;        // layer image captured when the tool started
    int m_srcX = 0, m_srcY = 0;
    QRectF m_box;           // source rectangle in layer-local coordinates
    QTransform m_t;         // layer-local -> canvas (current)
    QTransform m_startT;    // m_t at drag start
    QPointF m_pressCanvas;
    Drag m_drag = Drag::None;
    int m_corner = -1;
    double m_startAngle = 0;
};

// Eyedropper: click samples the composite. Alt sets the background colour.
class EyedropperTool : public Tool {
public:
    using Tool::Tool;
    ToolId id() const override { return ToolId::Eyedropper; }
    QString displayName() const override { return "Eyedropper"; }
    QString hint() const override { return "Click to pick a colour (Alt = background)."; }
    void mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) override;
};

// Brush and eraser (soft or hard round tips, clipped to the selection).
class BrushTool : public Tool {
public:
    BrushTool(ToolHost& h, bool eraser) : Tool(h), m_eraser(eraser) {}
    ToolId id() const override { return m_eraser ? ToolId::Eraser : ToolId::Brush; }
    QString displayName() const override { return m_eraser ? "Eraser" : "Brush"; }
    QString hint() const override { return "Drag to paint."; }
    void mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) override;
    void mouseMove(const QPointF& p, Qt::KeyboardModifiers mods) override;
    void mouseRelease(const QPointF& p, Qt::KeyboardModifiers mods) override;
    void drawOverlay(QPainter& p, double zoom) override;

private:
    bool m_eraser;
    bool m_dragging = false;
    QPointF m_last;
    QPointF m_cursor;
    bool m_hasCursor = false;
};

// Paint bucket: flood fill with the foreground colour.
class BucketTool : public Tool {
public:
    using Tool::Tool;
    ToolId id() const override { return ToolId::Bucket; }
    QString displayName() const override { return "Paint Bucket"; }
    QString hint() const override { return "Click to fill similar pixels with the foreground colour."; }
    void mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) override;
};

// Linear gradient from the foreground to the background colour.
class GradientTool : public Tool {
public:
    using Tool::Tool;
    ToolId id() const override { return ToolId::Gradient; }
    QString displayName() const override { return "Gradient"; }
    QString hint() const override { return "Drag to draw a gradient from foreground to background."; }
    void mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) override;
    void mouseMove(const QPointF& p, Qt::KeyboardModifiers mods) override;
    void mouseRelease(const QPointF& p, Qt::KeyboardModifiers mods) override;
    void drawOverlay(QPainter& p, double zoom) override;

private:
    QPointF m_start, m_cur;
    bool m_dragging = false;
};

// Raster text: click, type in the dialog; drawn with the foreground colour.
class TextTool : public Tool {
public:
    using Tool::Tool;
    ToolId id() const override { return ToolId::Text; }
    QString displayName() const override { return "Text"; }
    QString hint() const override { return "Click on the canvas to add text."; }
    void mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) override;
};

// Rectangle / ellipse shape drawn into the active layer.
class ShapeTool : public Tool {
public:
    ShapeTool(ToolHost& h, bool ellipse) : Tool(h), m_ellipse(ellipse) {}
    ToolId id() const override { return m_ellipse ? ToolId::ShapeEllipse : ToolId::ShapeRect; }
    QString displayName() const override { return m_ellipse ? "Ellipse" : "Rectangle"; }
    QString hint() const override { return "Drag to draw. Shift constrains to a square/circle."; }
    void mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) override;
    void mouseMove(const QPointF& p, Qt::KeyboardModifiers mods) override;
    void mouseRelease(const QPointF& p, Qt::KeyboardModifiers mods) override;
    void drawOverlay(QPainter& p, double zoom) override;

private:
    bool m_ellipse;
    QPointF m_start, m_cur;
    bool m_dragging = false;
    QRectF rectFor(Qt::KeyboardModifiers mods) const;
};

// Zoom tool: click zooms in, Alt-click zooms out.
class ZoomTool : public Tool {
public:
    using Tool::Tool;
    ToolId id() const override { return ToolId::Zoom; }
    QString displayName() const override { return "Zoom"; }
    QString hint() const override { return "Click to zoom in, Alt+click to zoom out."; }
    void mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) override;
};

// Hand tool: panning is handled by the canvas view.
class HandTool : public Tool {
public:
    using Tool::Tool;
    ToolId id() const override { return ToolId::Hand; }
    QString displayName() const override { return "Hand"; }
    QString hint() const override { return "Drag to pan the view."; }
};
