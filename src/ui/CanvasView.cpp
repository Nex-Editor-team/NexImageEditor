#include "ui/CanvasView.h"

#include "core/Ops.h"
#include "ui/Theme.h"

#include <QInputDialog>
#include <algorithm>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWheelEvent>
#include <cmath>

namespace {

constexpr double kMinZoom = 1.0 / 32;
constexpr double kMaxZoom = 32.0;

// Two states are "the same" when nothing observable changed; avoids empty undo entries.
bool sameState(const DocState& a, const DocState& b) {
    if (a.canvasW != b.canvasW || a.canvasH != b.canvasH || a.activeLayer != b.activeLayer) return false;
    if (a.selection != b.selection) return false;
    if (a.layers.size() != b.layers.size()) return false;
    for (size_t i = 0; i < a.layers.size(); ++i) {
        const Layer &x = a.layers[i], &y = b.layers[i];
        if (x.x != y.x || x.y != y.y || x.visible != y.visible || x.locked != y.locked ||
            x.opacity != y.opacity || x.blend != y.blend || x.name != y.name ||
            x.image.constBits() != y.image.constBits() || x.image.size() != y.image.size())
            return false;
    }
    return true;
}

}  // namespace

class EditCommand : public QUndoCommand {
public:
    EditCommand(CanvasView* view, const QString& text, DocState before, DocState after)
        : QUndoCommand(text), m_view(view), m_before(std::move(before)), m_after(std::move(after)) {}
    void undo() override { m_view->restoreState(m_before); }
    void redo() override {
        // The first redo is the push itself: the change is already applied.
        if (m_first) {
            m_first = false;
            return;
        }
        m_view->restoreState(m_after);
    }

private:
    CanvasView* m_view;
    DocState m_before, m_after;
    bool m_first = true;
};

CanvasView::CanvasView(Document doc, ToolOptions* options, QWidget* parent)
    : QWidget(parent), m_doc(std::move(doc)), m_opts(options) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setTool(ToolId::Move);
    m_antsTimer = new QTimer(this);
    m_antsTimer->setInterval(120);
    connect(m_antsTimer, &QTimer::timeout, this, [this] {
        m_antsPhase += 1;
        if (m_doc.selection) QWidget::update();
    });
    m_antsTimer->start();
    connect(&m_undo, &QUndoStack::indexChanged, this, [this] { emit changed(); QWidget::update(); });
}

CanvasView::~CanvasView() = default;

// ---------------------------------------------------------------- View

void CanvasView::setZoom(double z, const QPointF& anchorWidget) {
    z = std::clamp(z, kMinZoom, kMaxZoom);
    QPointF canvasAt = toCanvas(anchorWidget);
    m_zoom = z;
    m_origin = anchorWidget - canvasAt * m_zoom;
    clampOrigin();
    emit zoomChanged(m_zoom);
    QWidget::update();
}

void CanvasView::fitToWindow() {
    const double sx = (width() - 40.0) / m_doc.width();
    const double sy = (height() - 40.0) / m_doc.height();
    m_zoom = std::clamp(std::min(sx, sy), kMinZoom, 1.0);
    m_origin = QPointF((width() - m_doc.width() * m_zoom) / 2.0, (height() - m_doc.height() * m_zoom) / 2.0);
    emit zoomChanged(m_zoom);
    QWidget::update();
}

void CanvasView::clampOrigin() {
    // Keep at least a strip of the canvas visible.
    const double cw = m_doc.width() * m_zoom, ch = m_doc.height() * m_zoom;
    const double margin = 40;
    m_origin.setX(std::clamp(m_origin.x(), margin - cw, width() - margin));
    m_origin.setY(std::clamp(m_origin.y(), margin - ch, height() - margin));
}

QPointF CanvasView::toCanvas(const QPointF& w) const {
    return (w - m_origin) / m_zoom;
}

QPointF CanvasView::toWidget(const QPointF& c) const {
    return c * m_zoom + m_origin;
}

void CanvasView::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    if (!m_initialFit) {
        m_initialFit = true;
        fitToWindow();
    }
}

void CanvasView::setTool(ToolId id) {
    if (m_pending) commitEdit();
    if (m_tool) m_tool->cancel();
    m_toolId = id;
    m_tool = makeTool(id, *this);
    m_tool->begin();
    setCursor(id == ToolId::Hand ? Qt::OpenHandCursor : id == ToolId::Zoom ? Qt::CrossCursor : Qt::ArrowCursor);
    emit toolChanged(id);
    QWidget::update();
}

// ---------------------------------------------------------------- Painting

void CanvasView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Theme::canvasBackground());
    p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0);

    const QRectF canvasRect(0, 0, m_doc.width(), m_doc.height());
    QRectF screenRect(toWidget(canvasRect.topLeft()), toWidget(canvasRect.bottomRight()));

    // Drop shadow + checkerboard transparency.
    p.fillRect(screenRect.translated(3, 3), QColor(0, 0, 0, 90));
    {
        QImage checker(16, 16, QImage::Format_RGB32);
        checker.fill(QColor(0xcc, 0xcc, 0xcc));
        QPainter cp(&checker);
        cp.fillRect(0, 0, 8, 8, QColor(0xff, 0xff, 0xff));
        cp.fillRect(8, 8, 8, 8, QColor(0xff, 0xff, 0xff));
        cp.end();
        p.fillRect(screenRect, QBrush(checker));
    }

    p.save();
    p.translate(m_origin);
    p.scale(m_zoom, m_zoom);
    const bool hideActive = m_tool && m_tool->hidesActiveLayer();
    const int active = m_doc.activeIndex();
    const auto& layers = m_doc.layers();
    for (int i = 0; i < int(layers.size()); ++i) {
        const Layer& l = layers[size_t(i)];
        if (!l.visible) continue;
        if (hideActive && i == active) continue;
        p.setOpacity(l.opacity);
        p.setCompositionMode(blendComposition(l.blend));
        p.drawImage(l.x, l.y, l.image);
    }
    p.setOpacity(1.0);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    if (m_tool) m_tool->drawOverlay(p, m_zoom);

    // Pixel grid when zoomed in.
    if (m_zoom >= 8) {
        QPen grid(QColor(128, 128, 128, 70), 0);
        grid.setCosmetic(true);
        p.setPen(grid);
        for (int x = 0; x <= m_doc.width(); ++x) p.drawLine(QPointF(x, 0), QPointF(x, m_doc.height()));
        for (int y = 0; y <= m_doc.height(); ++y) p.drawLine(QPointF(0, y), QPointF(m_doc.width(), y));
    }

    // Marching ants for the selection.
    if (m_doc.selection && !m_doc.selection->outline.isEmpty()) {
        QPen black(Qt::black, 0), white(Qt::white, 0);
        black.setCosmetic(true);
        white.setCosmetic(true);
        black.setStyle(Qt::CustomDashLine);
        black.setDashPattern({4, 4});
        black.setDashOffset(m_antsPhase);
        white.setStyle(Qt::CustomDashLine);
        white.setDashPattern({4, 4});
        white.setDashOffset(m_antsPhase + 4);
        p.setBrush(Qt::NoBrush);
        p.setPen(white);
        p.drawPath(m_doc.selection->outline);
        p.setPen(black);
        p.drawPath(m_doc.selection->outline);
    }
    p.restore();

    QPen border(QColor(0x11, 0x11, 0x11), 1);
    p.setPen(border);
    p.setBrush(Qt::NoBrush);
    p.drawRect(screenRect);
}

// ---------------------------------------------------------------- Input

void CanvasView::mousePressEvent(QMouseEvent* e) {
    setFocus();
    const bool panMode = m_toolId == ToolId::Hand || m_spaceDown || e->button() == Qt::MiddleButton;
    if (panMode && (e->button() == Qt::LeftButton || e->button() == Qt::MiddleButton)) {
        m_panning = true;
        m_panLast = e->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (m_tool) m_tool->mousePress(toCanvas(e->position()), e->modifiers(), e->button());
    QWidget::update();
}

void CanvasView::mouseMoveEvent(QMouseEvent* e) {
    const QPoint pos = e->position().toPoint();
    emit pointerMoved(toCanvas(e->position()).toPoint());
    if (m_panning) {
        m_origin += QPointF(pos - m_panLast);
        m_panLast = pos;
        clampOrigin();
        QWidget::update();
        return;
    }
    if (m_tool) m_tool->mouseMove(toCanvas(e->position()), e->modifiers());
    QWidget::update();
}

void CanvasView::mouseReleaseEvent(QMouseEvent* e) {
    if (m_panning) {
        m_panning = false;
        setCursor(m_toolId == ToolId::Hand ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }
    if (m_tool) m_tool->mouseRelease(toCanvas(e->position()), e->modifiers());
    QWidget::update();
}

void CanvasView::wheelEvent(QWheelEvent* e) {
    const double steps = e->angleDelta().y() / 120.0;
    if (steps == 0) return;
    if (e->modifiers() & Qt::ControlModifier) {
        setZoom(m_zoom * std::pow(1.1, steps), e->position());
    } else {
        // Plain wheel pans vertically, Shift pans horizontally.
        QPointF d(0, e->angleDelta().y() / 2.0);
        if (e->modifiers() & Qt::ShiftModifier) d = QPointF(e->angleDelta().y() / 2.0, 0);
        m_origin += d;
        clampOrigin();
        QWidget::update();
    }
    e->accept();
}

void CanvasView::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        m_spaceDown = true;
        setCursor(Qt::OpenHandCursor);
        return;
    }
    if (m_tool && m_tool->keyPress(e->key())) {
        QWidget::update();
        return;
    }
    QWidget::keyPressEvent(e);
}

void CanvasView::keyReleaseEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        m_spaceDown = false;
        setCursor(m_toolId == ToolId::Hand ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }
    QWidget::keyReleaseEvent(e);
}

void CanvasView::leaveEvent(QEvent* e) {
    QWidget::leaveEvent(e);
    QWidget::update();
}

// ---------------------------------------------------------------- ToolHost

void CanvasView::beginEdit(const QString& name) {
    if (m_pending) return;  // nested brackets are not supported; keep the outermost
    m_pending = m_doc.snapshot();
    m_pendingName = name;
}

void CanvasView::commitEdit() {
    if (!m_pending) return;
    DocState before = std::move(*m_pending);
    m_pending.reset();
    DocState after = m_doc.snapshot();
    if (sameState(before, after)) return;
    m_undo.push(new EditCommand(this, m_pendingName, std::move(before), std::move(after)));
    emit changed();
}

void CanvasView::cancelEdit() {
    if (!m_pending) return;
    restoreState(*m_pending);
    m_pending.reset();
}

void CanvasView::documentChanged() {
    clampOrigin();
    emit changed();
    QWidget::update();
}

void CanvasView::setStatus(const QString& text) {
    emit statusMessage(text);
}

QString CanvasView::promptText() {
    bool ok = false;
    QString text = QInputDialog::getMultiLineText(this, "Text", "Enter text:", QString(), &ok);
    return ok ? text : QString();
}

void CanvasView::zoomAt(const QPointF& canvasPos, bool zoomIn) {
    Q_UNUSED(canvasPos)
    setZoom(m_zoom * (zoomIn ? 2.0 : 0.5), toWidget(canvasPos));
}

void CanvasView::restoreState(const DocState& s) {
    m_doc.restore(s);
    emit changed();
    QWidget::update();
}

// ---------------------------------------------------------------- Document-level edits

void CanvasView::editDocument(const QString& name, const std::function<void(Document&)>& fn) {
    beginEdit(name);
    fn(m_doc);
    commitEdit();
    clampOrigin();
    QWidget::update();
}

void CanvasView::selectLayer(int index) {
    if (index < 0 || index >= int(m_doc.layers().size())) return;
    m_doc.setActive(index);
    emit changed();
}

void CanvasView::addLayer() {
    editDocument("New Layer", [](Document& d) {
        d.addLayer(QString("Layer %1").arg(d.layers().size()), newImage(d.width(), d.height()));
    });
}

void CanvasView::duplicateLayer() {
    const int i = m_doc.activeIndex();
    editDocument("Duplicate Layer", [i](Document& d) { d.duplicateLayer(i); });
}

void CanvasView::deleteLayer() {
    const int i = m_doc.activeIndex();
    if (m_doc.layers().size() <= 1) return;
    editDocument("Delete Layer", [i](Document& d) { d.removeLayer(i); });
}

void CanvasView::mergeDownLayer() {
    const int i = m_doc.activeIndex();
    if (i <= 0) return;
    editDocument("Merge Down", [i](Document& d) { d.mergeDown(i); });
}

void CanvasView::moveLayer(int from, int to) {
    editDocument("Move Layer", [from, to](Document& d) { d.moveLayer(from, to); });
}

void CanvasView::editLayer(int index, const QString& name, const std::function<void(Layer&)>& fn) {
    editDocument(name, [index, &fn](Document& d) {
        if (index >= 0 && index < int(d.layers().size())) fn(d.layers()[size_t(index)]);
    });
}

void CanvasView::flattenImage() {
    editDocument("Flatten Image", [](Document& d) { d.flattenAll(); });
}
