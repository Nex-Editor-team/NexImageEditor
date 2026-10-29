#pragma once
#include <QWidget>
#include <QUndoStack>
#include <QVector>
#include <QPointF>
#include "../project/Project.h"
#include "../engine/Transform.h"

enum class ToolMode { Select, FreeTransform, Crop, Pan };

// The central editing surface. Owns no data itself (Project/QUndoStack
// are borrowed from MainWindow) - it just renders the project and turns
// mouse input into either live transform previews or committed
// QUndoCommands.
class Canvas : public QWidget {
    Q_OBJECT
public:
    explicit Canvas(QWidget* parent = nullptr);

    void setProject(Project* project);
    void setUndoStack(QUndoStack* stack) { m_undoStack = stack; }

    void setSelectedLayer(Layer* layer);
    Layer* selectedLayer() const { return m_selected; }

    void setTool(ToolMode mode);
    ToolMode tool() const { return m_tool; }

    void setZoom(double zoom);
    double zoom() const { return m_zoom; }
    void zoomToFit();

signals:
    void selectionChanged(Layer* layer);
    void zoomChanged(double zoom);
    void transformLiveChanged(Layer* layer); // fired continuously while dragging (for Properties panel)

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    // --- coordinate mapping ---
    QPointF canvasOrigin() const; // top-left of the canvas in widget coords
    QPointF widgetToCanvas(const QPointF& p) const;
    QPointF canvasToWidget(const QPointF& p) const;

    // --- free transform interaction ---
    void drawCheckerboard(QPainter& painter) const;
    void drawSelectionOverlay(QPainter& painter) const;
    Transform::TransformHandle hitTestHandle(const QPointF& widgetPos) const;
    QVector<QPointF> handlePositionsWidget() const; // 8 box handles, widget space
    QPointF rotateHandlePositionWidget() const;
    Layer* hitTestLayer(const QPointF& canvasPos) const;

    void beginDrag(const QPointF& widgetPos);
    void updateDrag(const QPointF& widgetPos, Qt::KeyboardModifiers mods);
    void endDrag();

    Project* m_project = nullptr;
    QUndoStack* m_undoStack = nullptr;
    Layer* m_selected = nullptr;

    ToolMode m_tool = ToolMode::Select;
    double m_zoom = 1.0;
    QPointF m_panOffset{0, 0};
    bool m_panning = false;
    QPointF m_panStart;
    QPointF m_panStartOffset;

    // drag state
    bool m_dragging = false;
    Transform::TransformHandle m_activeHandle = Transform::TransformHandle::None;
    Transform::FreeTransformState m_dragStartState;
    QPointF m_dragStartWidgetPos;
    QPointF m_dragStartCanvasCenter;
    double m_dragStartAngleToMouse = 0.0;
};
