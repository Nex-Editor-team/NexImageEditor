#pragma once
#include "core/Document.h"
#include "core/Selection.h"

#include <QColor>
#include <QPainter>
#include <QString>
#include <memory>

enum class ToolId {
    Move, RectMarquee, EllipseMarquee, Lasso, MagicWand, Crop, Eyedropper,
    Brush, Eraser, Bucket, Gradient, Text, ShapeRect, ShapeEllipse,
    Transform, Zoom, Hand
};

struct ToolOptions {
    QColor fg = Qt::black;
    QColor bg = Qt::white;
    double brushSize = 12;
    double hardness = 1.0;    // 0..1
    int tolerance = 32;       // 0..255
    bool contiguous = true;   // flood fill / wand
    SelOp selOp = SelOp::Replace;
    QString fontFamily = "Sans";
    int fontSize = 48;
    bool shapeStroke = false;
    double strokeWidth = 4;
};

// Services the canvas and main window provide to tools.
class ToolHost {
public:
    virtual ~ToolHost() = default;
    virtual Document& doc() = 0;
    virtual ToolOptions& options() = 0;
    // Undo bracket: beginEdit snapshots the state, commitEdit pushes an undo entry.
    virtual void beginEdit(const QString& name) = 0;
    virtual void commitEdit() = 0;
    virtual void cancelEdit() = 0;
    virtual void update() = 0;           // repaint canvas
    virtual void documentChanged() = 0;  // layers / size changed: refresh panels
    virtual void setStatus(const QString& text) = 0;
    virtual QString promptText() = 0;    // empty string = cancelled
    virtual void zoomAt(const QPointF& canvasPos, bool zoomIn) = 0;
};

class Tool {
public:
    explicit Tool(ToolHost& host) : m_host(host) {}
    virtual ~Tool() = default;

    virtual ToolId id() const = 0;
    virtual QString displayName() const = 0;
    virtual QString hint() const { return {}; }

    // Called when the tool becomes active.
    virtual void begin() {}
    // Called when the tool is deactivated; uncommitted work is discarded.
    virtual void cancel() {}

    virtual void mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton button) {
        Q_UNUSED(p) Q_UNUSED(mods) Q_UNUSED(button)
    }
    virtual void mouseMove(const QPointF& p, Qt::KeyboardModifiers mods) {
        Q_UNUSED(p) Q_UNUSED(mods)
    }
    virtual void mouseRelease(const QPointF& p, Qt::KeyboardModifiers mods) {
        Q_UNUSED(p) Q_UNUSED(mods)
    }
    virtual bool keyPress(int key) {
        Q_UNUSED(key)
        return false;
    }
    // Drawn in canvas coordinates on top of the composite.
    virtual void drawOverlay(QPainter& p, double zoom) {
        Q_UNUSED(p) Q_UNUSED(zoom)
    }
    // True while the tool draws the active layer itself (e.g. transform preview).
    virtual bool hidesActiveLayer() const { return false; }

protected:
    ToolHost& m_host;
};

std::unique_ptr<Tool> makeTool(ToolId id, ToolHost& host);
QString toolName(ToolId id);
