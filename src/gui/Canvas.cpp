#include "Canvas.h"
#include "../project/Commands.h"
#include <QPainter>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QUrl>
#include "../io/ImageIO.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPolygonF>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {
// Only accept local image files. Never fetch remote URLs or move source files.
QStringList droppedImagePaths(const QMimeData* mime) {
    QStringList paths;
    if (!mime || !mime->hasUrls()) return paths;
    for (const QUrl& url : mime->urls()) {
        if (!url.isLocalFile()) continue;
        const QString path = url.toLocalFile();
        if (ImageIO::isSupportedExtension(path) && QFileInfo(path).isFile())
            paths.append(path);
    }
    paths.removeDuplicates();
    return paths;
}

constexpr double kHandleRadiusPx = 6.0;      // hit-test radius for box handles, in widget px
constexpr double kRotateHandleOffsetPx = 28.0; // distance of rotate handle above the box, widget px
}

Canvas::Canvas(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
}

void Canvas::setProject(Project* project) {
    m_project = project;
    m_selected = nullptr;
    zoomToFit();
    update();
}

void Canvas::setSelectedLayer(Layer* layer) {
    if (m_selected == layer) return;
    m_selected = layer;
    emit selectionChanged(layer);
    update();
}

void Canvas::setTool(ToolMode mode) {
    m_tool = mode;
    update();
}

void Canvas::setZoom(double zoom) {
    m_zoom = std::clamp(zoom, 0.02, 40.0);
    update();
}

void Canvas::zoomToFit() {
    if (!m_project) return;
    QSize cs = m_project->canvasSize();
    if (cs.isEmpty() || width() <= 0 || height() <= 0) { m_zoom = 1.0; return; }
    double zx = (width() - 40) / double(cs.width());
    double zy = (height() - 40) / double(cs.height());
    m_zoom = std::clamp(std::min(zx, zy), 0.02, 40.0);
    m_panOffset = {0, 0};
}

// ---------------- coordinate mapping ----------------

QPointF Canvas::canvasOrigin() const {
    if (!m_project) return {0, 0};
    QSize cs = m_project->canvasSize();
    return QPointF(width() / 2.0, height() / 2.0)
           - QPointF(cs.width() * m_zoom / 2.0, cs.height() * m_zoom / 2.0)
           + m_panOffset;
}

QPointF Canvas::widgetToCanvas(const QPointF& p) const {
    return (p - canvasOrigin()) / m_zoom;
}

QPointF Canvas::canvasToWidget(const QPointF& p) const {
    return canvasOrigin() + p * m_zoom;
}

// ---------------- painting ----------------

void Canvas::drawCheckerboard(QPainter& painter) const {
    if (!m_project) return;
    QRectF r(canvasOrigin(), QSizeF(m_project->canvasSize()) * m_zoom);
    const int cell = 12;
    painter.save();
    painter.setClipRect(r);
    for (int y = int(r.top()); y < int(r.bottom()) + cell; y += cell) {
        for (int x = int(r.left()); x < int(r.right()) + cell; x += cell) {
            bool dark = ((x / cell) + (y / cell)) % 2 == 0;
            painter.fillRect(QRectF(x, y, cell, cell), dark ? QColor(210, 210, 210) : QColor(240, 240, 240));
        }
    }
    painter.restore();
    painter.setPen(QPen(QColor(90, 90, 90), 1));
    painter.drawRect(r);
}

void Canvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(50, 50, 54));
    if (!m_project) return;

    drawCheckerboard(painter);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.translate(canvasOrigin());
    painter.scale(m_zoom, m_zoom);
    painter.setClipRect(QRectF(QPointF(0, 0), QSizeF(m_project->canvasSize())));
    m_project->render(painter);
    painter.restore();

    if (m_selected && (m_tool == ToolMode::Select || m_tool == ToolMode::FreeTransform))
        drawSelectionOverlay(painter);
}

void Canvas::drawSelectionOverlay(QPainter& painter) const {
    if (!m_selected) return;

    QVector<QPointF> handles = handlePositionsWidget(); // TL,TM,TR,ML,MR,BL,BM,BR (widget space)
    QPolygonF box;
    box << handles[0] << handles[2] << handles[7] << handles[5]; // TL,TR,BR,BL

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen boxPen(QColor(60, 160, 255), 1.5);
    painter.setPen(boxPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(box);

    // rotate handle + connecting line
    QPointF topMid = handles[1];
    QPointF rotateHandle = rotateHandlePositionWidget();
    painter.drawLine(topMid, rotateHandle);

    painter.setBrush(Qt::white);
    for (const QPointF& h : handles) {
        painter.drawRect(QRectF(h.x() - kHandleRadiusPx / 2, h.y() - kHandleRadiusPx / 2,
                                 kHandleRadiusPx, kHandleRadiusPx));
    }
    painter.setBrush(QColor(60, 160, 255));
    painter.drawEllipse(rotateHandle, kHandleRadiusPx / 2 + 1, kHandleRadiusPx / 2 + 1);

    painter.restore();
}

// ---------------- handle geometry ----------------

QVector<QPointF> Canvas::handlePositionsWidget() const {
    QVector<QPointF> out;
    if (!m_selected) return out;
    QTransform m = m_selected->matrix();
    QSizeF sz(m_selected->content.size());

    // local content-space points, in the same order used everywhere else:
    // TL, TM, TR, ML, MR, BL, BM, BR
    QVector<QPointF> local = {
        {0, 0}, {sz.width() / 2, 0}, {sz.width(), 0},
        {0, sz.height() / 2}, {sz.width(), sz.height() / 2},
        {0, sz.height()}, {sz.width() / 2, sz.height()}, {sz.width(), sz.height()}
    };
    for (const QPointF& lp : local)
        out.push_back(canvasToWidget(m.map(lp)));
    return out;
}

QPointF Canvas::rotateHandlePositionWidget() const {
    QVector<QPointF> handles = handlePositionsWidget();
    if (handles.isEmpty()) return {};
    QPointF topMid = handles[1];
    QPointF center = handles.size() > 4 ? (handles[0] + handles[7]) / 2.0 : topMid;
    QPointF dir = topMid - center;
    double len = std::hypot(dir.x(), dir.y());
    if (len < 1e-6) return topMid + QPointF(0, -kRotateHandleOffsetPx);
    dir /= len;
    return topMid + dir * kRotateHandleOffsetPx;
}

Transform::TransformHandle Canvas::hitTestHandle(const QPointF& widgetPos) const {
    if (!m_selected) return Transform::TransformHandle::None;

    if (std::hypot(widgetPos.x() - rotateHandlePositionWidget().x(),
                    widgetPos.y() - rotateHandlePositionWidget().y()) <= kHandleRadiusPx + 2)
        return Transform::TransformHandle::Rotate;

    QVector<QPointF> h = handlePositionsWidget();
    static const Transform::TransformHandle order[8] = {
        Transform::TransformHandle::TopLeft, Transform::TransformHandle::TopMiddle, Transform::TransformHandle::TopRight,
        Transform::TransformHandle::MiddleLeft, Transform::TransformHandle::MiddleRight,
        Transform::TransformHandle::BottomLeft, Transform::TransformHandle::BottomMiddle, Transform::TransformHandle::BottomRight
    };
    for (int i = 0; i < h.size(); ++i) {
        if (std::hypot(widgetPos.x() - h[i].x(), widgetPos.y() - h[i].y()) <= kHandleRadiusPx + 2)
            return order[i];
    }

    QPolygonF box;
    box << h[0] << h[2] << h[7] << h[5];
    if (box.containsPoint(widgetPos, Qt::OddEvenFill))
        return Transform::TransformHandle::Body;

    return Transform::TransformHandle::None;
}

Layer* Canvas::hitTestLayer(const QPointF& canvasPos) const {
    if (!m_project) return nullptr;
    const auto& layers = m_project->layers();
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        Layer* l = it->get();
        if (!l->visible || l->locked || l->content.isNull()) continue;
        QPointF local = l->matrix().inverted().map(canvasPos);
        if (QRectF(QPointF(0, 0), QSizeF(l->content.size())).contains(local))
            return l;
    }
    return nullptr;
}

// ---------------- mouse interaction ----------------

void Canvas::mousePressEvent(QMouseEvent* event) {
    if (!m_project || event->button() != Qt::LeftButton) return;

    if (m_tool == ToolMode::Pan) return; // panning handled by drag delta below via generic path

    Transform::TransformHandle handle = Transform::TransformHandle::None;
    if (m_selected && (m_tool == ToolMode::Select || m_tool == ToolMode::FreeTransform))
        handle = hitTestHandle(event->position());

    if (handle == Transform::TransformHandle::None) {
        Layer* hit = hitTestLayer(widgetToCanvas(event->position()));
        setSelectedLayer(hit);
        if (!hit) return;
        handle = Transform::TransformHandle::Body;
    }

    m_activeHandle = handle;
    beginDrag(event->position());
}

void Canvas::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging) {
        updateDrag(event->position(), event->modifiers());
        return;
    }
    // Just hovering: could set cursor shape per-handle here (omitted for brevity).
}

void Canvas::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    if (m_dragging) endDrag();
}

void Canvas::wheelEvent(QWheelEvent* event) {
    double factor = event->angleDelta().y() > 0 ? 1.1 : (1.0 / 1.1);
    setZoom(m_zoom * factor);
}

void Canvas::beginDrag(const QPointF& widgetPos) {
    if (!m_selected) return;
    m_dragging = true;
    m_dragStartState = m_selected->transform;
    m_dragStartWidgetPos = widgetPos;
    m_dragStartCanvasCenter = m_dragStartState.center;

    QPointF startCanvas = widgetToCanvas(widgetPos);
    QPointF v0 = startCanvas - m_dragStartState.center;
    m_dragStartAngleToMouse = std::atan2(v0.y(), v0.x());
}

void Canvas::updateDrag(const QPointF& widgetPos, Qt::KeyboardModifiers mods) {
    if (!m_selected) return;
    using TH = Transform::TransformHandle;

    Transform::FreeTransformState state = m_dragStartState;
    QPointF mouseCanvas = widgetToCanvas(widgetPos);
    QPointF startCanvas = widgetToCanvas(m_dragStartWidgetPos);

    if (m_activeHandle == TH::Body) {
        QPointF delta = mouseCanvas - startCanvas;
        state.center = m_dragStartState.center + delta;

    } else if (m_activeHandle == TH::Rotate) {
        QPointF v = mouseCanvas - m_dragStartState.center;
        double angle = qRadiansToDegrees(std::atan2(v.y(), v.x()));
        double startAngle = qRadiansToDegrees(m_dragStartAngleToMouse);
        double delta = angle - startAngle;
        state.rotationDeg = m_dragStartState.rotationDeg + delta;
        if (mods & Qt::ShiftModifier) // snap to 15 degree steps
            state.rotationDeg = std::round(state.rotationDeg / 15.0) * 15.0;

    } else {
        // Corner/edge scale handles: work in the content's local (unrotated,
        // unsheared) axis so dragging feels natural regardless of rotation.
        QTransform rs; // rotate+shear only, no scale/translate
        rs.rotate(m_dragStartState.rotationDeg);
        rs.shear(std::tan(qDegreesToRadians(m_dragStartState.shearXDeg)), 0.0);
        QTransform rsInv = rs.inverted();

        QPointF x0 = rsInv.map(startCanvas - m_dragStartState.center);
        QPointF x1 = rsInv.map(mouseCanvas - m_dragStartState.center);

        auto ratio = [](double a, double b) {
            if (std::abs(a) < 1e-4) return 1.0;
            return b / a;
        };

        bool affectsX = (m_activeHandle == TH::TopLeft || m_activeHandle == TH::TopRight ||
                          m_activeHandle == TH::BottomLeft || m_activeHandle == TH::BottomRight ||
                          m_activeHandle == TH::MiddleLeft || m_activeHandle == TH::MiddleRight);
        bool affectsY = (m_activeHandle == TH::TopLeft || m_activeHandle == TH::TopRight ||
                          m_activeHandle == TH::BottomLeft || m_activeHandle == TH::BottomRight ||
                          m_activeHandle == TH::TopMiddle || m_activeHandle == TH::BottomMiddle);

        if (affectsX) {
            double rX = ratio(x0.x(), x1.x());
            state.scaleX = m_dragStartState.scaleX * rX;
        }
        if (affectsY) {
            double rY = ratio(x0.y(), x1.y());
            state.scaleY = m_dragStartState.scaleY * rY;
        }

        // Shift = keep proportions locked to the original aspect ratio.
        if ((mods & Qt::ShiftModifier) && affectsX && affectsY) {
            double avg = (std::abs(state.scaleX / m_dragStartState.scaleX) +
                          std::abs(state.scaleY / m_dragStartState.scaleY)) / 2.0;
            state.scaleX = m_dragStartState.scaleX * avg * (state.scaleX < 0 ? -1 : 1);
            state.scaleY = m_dragStartState.scaleY * avg * (state.scaleY < 0 ? -1 : 1);
        }

        // Guard against collapsing to zero (still allows flipping via sign).
        if (std::abs(state.scaleX) < 0.02) state.scaleX = state.scaleX < 0 ? -0.02 : 0.02;
        if (std::abs(state.scaleY) < 0.02) state.scaleY = state.scaleY < 0 ? -0.02 : 0.02;
    }

    m_selected->transform = state; // live preview, not yet committed to undo stack
    emit transformLiveChanged(m_selected);
    update();
}

void Canvas::endDrag() {
    m_dragging = false;
    if (!m_selected || !m_undoStack) return;

    if (m_selected->transform != m_dragStartState) {
        auto* cmd = new TransformLayerCommand(m_project, m_selected->id(), m_dragStartState, m_selected->transform);
        // The state is already live on the layer; push the command with
        // a matching redo() so the undo stack's own state stays consistent.
        m_undoStack->push(cmd);
    }
    m_activeHandle = Transform::TransformHandle::None;
}

void Canvas::dragEnterEvent(QDragEnterEvent* event) {
    if (m_project && event->possibleActions().testFlag(Qt::CopyAction)
        && !droppedImagePaths(event->mimeData()).isEmpty()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    } else {
        event->ignore();
    }
}

void Canvas::dragMoveEvent(QDragMoveEvent* event) {
    if (m_project && event->possibleActions().testFlag(Qt::CopyAction)
        && !droppedImagePaths(event->mimeData()).isEmpty()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    } else {
        event->ignore();
    }
}

void Canvas::dropEvent(QDropEvent* event) {
    const QStringList paths = droppedImagePaths(event->mimeData());
    if (!m_project || !event->possibleActions().testFlag(Qt::CopyAction) || paths.isEmpty()) {
        event->ignore();
        return;
    }
    const QPointF position = widgetToCanvas(event->position());
    event->setDropAction(Qt::CopyAction);
    event->accept();
    emit imageFilesDropped(paths, position);
}

void Canvas::contextMenuEvent(QContextMenuEvent* event) {
    if (!m_project || m_dragging) { event->ignore(); return; }
    // Keyboard invocation targets the selection; mouse invocation targets the hit layer.
    Layer* target = event->reason() == QContextMenuEvent::Keyboard
        ? m_selected : hitTestLayer(widgetToCanvas(event->pos()));
    const QPoint globalPosition = event->reason() == QContextMenuEvent::Keyboard
        ? mapToGlobal(rect().center()) : event->globalPos();
    emit layerContextMenuRequested(target, globalPosition);
    event->accept();
}
