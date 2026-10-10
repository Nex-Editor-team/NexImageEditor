#include "tools/Tools.h"

#include "core/Ops.h"

#include <QPainterPathStroker>
#include <algorithm>
#include <cmath>

namespace {

Layer* editableActive(ToolHost& h) {
    Layer* l = h.doc().active();
    if (!l) return nullptr;
    if (l->locked) {
        h.setStatus("The active layer is locked.");
        return nullptr;
    }
    return l;
}

// Canvas-space mask (w*h) of the layer's own coverage at the given canvas size.
std::vector<uint8_t> canvasMaskFromLayer(const Layer& layer, const std::vector<uint8_t>& lm, int w, int h) {
    std::vector<uint8_t> out(size_t(w) * size_t(h), 0);
    for (int y = 0; y < layer.h(); ++y)
        for (int x = 0; x < layer.w(); ++x) {
            int cx = x + layer.x, cy = y + layer.y;
            if (cx < 0 || cy < 0 || cx >= w || cy >= h) continue;
            out[size_t(cy) * size_t(w) + size_t(cx)] = lm[size_t(y) * size_t(layer.w()) + size_t(x)];
        }
    return out;
}

void applySelectionOp(ToolHost& h, const std::shared_ptr<Selection>& added, SelOp op) {
    Document& d = h.doc();
    h.beginEdit("Selection");
    d.selection = combineSelections(d.selection, added, op, d.width(), d.height());
    h.commitEdit();
    h.update();
}

}  // namespace

QString toolName(ToolId id) {
    switch (id) {
    case ToolId::Move: return "Move";
    case ToolId::RectMarquee: return "Rectangular Marquee";
    case ToolId::EllipseMarquee: return "Elliptical Marquee";
    case ToolId::Lasso: return "Lasso";
    case ToolId::MagicWand: return "Magic Wand";
    case ToolId::Crop: return "Crop";
    case ToolId::Eyedropper: return "Eyedropper";
    case ToolId::Brush: return "Brush";
    case ToolId::Eraser: return "Eraser";
    case ToolId::Bucket: return "Paint Bucket";
    case ToolId::Gradient: return "Gradient";
    case ToolId::Text: return "Text";
    case ToolId::ShapeRect: return "Rectangle";
    case ToolId::ShapeEllipse: return "Ellipse";
    case ToolId::Transform: return "Free Transform";
    case ToolId::Zoom: return "Zoom";
    case ToolId::Hand: return "Hand";
    }
    return {};
}

std::unique_ptr<Tool> makeTool(ToolId id, ToolHost& host) {
    switch (id) {
    case ToolId::Move: return std::make_unique<MoveTool>(host);
    case ToolId::RectMarquee:
    case ToolId::EllipseMarquee:
    case ToolId::Lasso: return std::make_unique<MarqueeTool>(host, id);
    case ToolId::MagicWand: return std::make_unique<MagicWandTool>(host);
    case ToolId::Crop: return std::make_unique<CropTool>(host);
    case ToolId::Eyedropper: return std::make_unique<EyedropperTool>(host);
    case ToolId::Brush: return std::make_unique<BrushTool>(host, false);
    case ToolId::Eraser: return std::make_unique<BrushTool>(host, true);
    case ToolId::Bucket: return std::make_unique<BucketTool>(host);
    case ToolId::Gradient: return std::make_unique<GradientTool>(host);
    case ToolId::Text: return std::make_unique<TextTool>(host);
    case ToolId::ShapeRect: return std::make_unique<ShapeTool>(host, false);
    case ToolId::ShapeEllipse: return std::make_unique<ShapeTool>(host, true);
    case ToolId::Transform: return std::make_unique<TransformTool>(host);
    case ToolId::Zoom: return std::make_unique<ZoomTool>(host);
    case ToolId::Hand: return std::make_unique<HandTool>(host);
    }
    return nullptr;
}

// ---------------------------------------------------------------- Marquee

MarqueeTool::MarqueeTool(ToolHost& h, ToolId id) : Tool(h), m_id(id) {}

QString MarqueeTool::displayName() const { return toolName(m_id); }

QString MarqueeTool::hint() const {
    return "Drag to select. Shift adds to the selection, Alt subtracts from it.";
}

SelOp MarqueeTool::opFromMods(Qt::KeyboardModifiers mods) const {
    if (mods & Qt::ShiftModifier) return SelOp::Add;
    if (mods & Qt::AltModifier) return SelOp::Subtract;
    return SelOp::Replace;
}

void MarqueeTool::mousePress(const QPointF& p, Qt::KeyboardModifiers, Qt::MouseButton b) {
    if (b != Qt::LeftButton) return;
    m_start = m_cur = p;
    m_dragging = true;
    m_lasso.clear();
    m_lasso << p;
}

void MarqueeTool::mouseMove(const QPointF& p, Qt::KeyboardModifiers mods) {
    if (!m_dragging) return;
    m_cur = p;
    if (m_id == ToolId::Lasso) m_lasso << p;
    Q_UNUSED(mods)
    m_host.update();
}

void MarqueeTool::mouseRelease(const QPointF& p, Qt::KeyboardModifiers mods) {
    if (!m_dragging) return;
    m_dragging = false;
    m_cur = p;
    Document& d = m_host.doc();
    const int W = d.width(), H = d.height();
    SelOp op = opFromMods(mods);
    std::shared_ptr<Selection> sel;
    if (m_id == ToolId::Lasso) {
        if (m_lasso.size() < 3) {
            m_host.update();
            return;
        }
        sel = Selection::fromMask(Ops::polygonMask(m_lasso, W, H), W, H);
    } else {
        QRect r = QRectF(m_start, m_cur).normalized().toAlignedRect();
        if (r.width() < 1 || r.height() < 1) {
            // A plain click deselects.
            if (op == SelOp::Replace) {
                m_host.beginEdit("Deselect");
                d.selection.reset();
                m_host.commitEdit();
            }
            m_host.update();
            return;
        }
        sel = m_id == ToolId::EllipseMarquee ? Selection::fromMask(Ops::ellipseMask(r, W, H), W, H)
                                             : Selection::fromMask(Ops::rectMask(r, W, H), W, H);
    }
    applySelectionOp(m_host, sel, op);
}

void MarqueeTool::drawOverlay(QPainter& p, double zoom) {
    Q_UNUSED(zoom)
    if (!m_dragging) return;
    QPen pen(Qt::black, 0);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    if (m_id == ToolId::Lasso) {
        if (m_lasso.size() > 1) p.drawPolyline(m_lasso);
    } else {
        QRectF r = QRectF(m_start, m_cur).normalized();
        if (m_id == ToolId::EllipseMarquee) p.drawEllipse(r);
        else p.drawRect(r);
    }
}

// ---------------------------------------------------------------- Magic wand

void MagicWandTool::mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) {
    if (b != Qt::LeftButton) return;
    Document& d = m_host.doc();
    QPoint seed = p.toPoint();
    if (seed.x() < 0 || seed.y() < 0 || seed.x() >= d.width() || seed.y() >= d.height()) return;
    auto mask = Ops::floodMask(d.composite(), seed, m_host.options().tolerance, m_host.options().contiguous);
    SelOp op = (mods & Qt::ShiftModifier) ? SelOp::Add : (mods & Qt::AltModifier) ? SelOp::Subtract : SelOp::Replace;
    applySelectionOp(m_host, Selection::fromMask(std::move(mask), d.width(), d.height()), op);
}

// ---------------------------------------------------------------- Move

void MoveTool::cancel() {
    if (m_dragging && !m_selMode) {
        if (Layer* l = m_host.doc().active()) {
            l->x = m_layerX;
            l->y = m_layerY;
        }
        m_host.cancelEdit();
    }
    m_dragging = false;
    m_selMode = false;
}

void MoveTool::mousePress(const QPointF& p, Qt::KeyboardModifiers, Qt::MouseButton b) {
    if (b != Qt::LeftButton) return;
    Layer* l = editableActive(m_host);
    if (!l) return;
    Document& d = m_host.doc();
    m_start = p;
    m_offset = QPointF();
    m_dragging = true;
    m_layerX = l->x;
    m_layerY = l->y;
    m_selMode = d.selection && d.selection->at(int(p.x()), int(p.y())) > 0;
    m_host.beginEdit("Move");
    if (m_selMode) {
        auto lm = Ops::layerMaskFromSelection(*l, d.selection.get());
        Pixels src = toPixels(l->image), cut = src, fl = src;
        for (int y = 0; y < src.h; ++y)
            for (int x = 0; x < src.w; ++x) {
                int a = lm[size_t(y) * size_t(src.w) + size_t(x)];
                for (int c = 0; c < 4; ++c) {
                    fl.px(x, y)[c] = uint8_t(src.px(x, y)[c] * a / 255);
                    cut.px(x, y)[c] = uint8_t(src.px(x, y)[c] * (255 - a) / 255);
                }
            }
        m_cut = fromPixels(cut);
        m_floating = fromPixels(fl);
    }
}

void MoveTool::mouseMove(const QPointF& p, Qt::KeyboardModifiers) {
    if (!m_dragging) return;
    Layer* l = m_host.doc().active();
    if (!l) return;
    m_offset = p - m_start;
    if (!m_selMode) {
        l->x = m_layerX + int(std::lround(m_offset.x()));
        l->y = m_layerY + int(std::lround(m_offset.y()));
    }
    m_host.update();
}

void MoveTool::mouseRelease(const QPointF&, Qt::KeyboardModifiers) {
    if (!m_dragging) return;
    m_dragging = false;
    Document& d = m_host.doc();
    Layer* l = d.active();
    const int dx = int(std::lround(m_offset.x())), dy = int(std::lround(m_offset.y()));
    if (!l || (dx == 0 && dy == 0)) {
        m_host.cancelEdit();
        m_selMode = false;
        m_host.update();
        return;
    }
    if (m_selMode && d.selection) {
        Ops::moveSelectedPixels(*l, *d.selection, dx, dy);
        // Shift the selection outline with the pixels.
        std::vector<uint8_t> nm(size_t(d.width()) * size_t(d.height()), 0);
        for (int y = 0; y < d.height(); ++y)
            for (int x = 0; x < d.width(); ++x) {
                int sx = x - dx, sy = y - dy;
                if (sx < 0 || sy < 0 || sx >= d.width() || sy >= d.height()) continue;
                nm[size_t(y) * size_t(d.width()) + size_t(x)] = d.selection->at(sx, sy);
            }
        d.selection = Selection::fromMask(std::move(nm), d.width(), d.height());
    }
    m_selMode = false;
    m_cut = QImage();
    m_floating = QImage();
    m_host.commitEdit();
    m_host.documentChanged();
    m_host.update();
}

void MoveTool::drawOverlay(QPainter& p, double zoom) {
    Q_UNUSED(zoom)
    if (!m_selMode || !m_dragging) return;
    Layer* l = m_host.doc().active();
    if (!l) return;
    p.drawImage(l->x, l->y, m_cut);
    p.drawImage(l->x + int(std::lround(m_offset.x())), l->y + int(std::lround(m_offset.y())), m_floating);
}

// ---------------------------------------------------------------- Crop

void CropTool::mousePress(const QPointF& p, Qt::KeyboardModifiers, Qt::MouseButton b) {
    if (b != Qt::LeftButton) return;
    m_start = p;
    m_rect = QRectF(p, p);
    m_dragging = true;
}

void CropTool::mouseMove(const QPointF& p, Qt::KeyboardModifiers) {
    if (!m_dragging) return;
    m_rect = QRectF(m_start, p).normalized();
    m_host.update();
}

void CropTool::mouseRelease(const QPointF& p, Qt::KeyboardModifiers) {
    if (!m_dragging) return;
    m_dragging = false;
    m_rect = QRectF(m_start, p).normalized();
    m_host.update();
}

bool CropTool::keyPress(int key) {
    if (key == Qt::Key_Escape) {
        cancel();
        m_host.update();
        return true;
    }
    if (key != Qt::Key_Return && key != Qt::Key_Enter) return false;
    Document& d = m_host.doc();
    QRect r = m_rect.toAlignedRect().intersected(QRect(0, 0, d.width(), d.height()));
    if (r.width() < 1 || r.height() < 1) return true;
    m_host.beginEdit("Crop");
    Ops::cropDocument(d, r);
    m_host.commitEdit();
    cancel();
    m_host.documentChanged();
    m_host.update();
    return true;
}

void CropTool::drawOverlay(QPainter& p, double zoom) {
    Q_UNUSED(zoom)
    if (m_rect.isNull()) return;
    Document& d = m_host.doc();
    QPainterPath outside;
    outside.addRect(QRectF(0, 0, d.width(), d.height()));
    outside.addRect(m_rect);
    p.fillPath(outside, QColor(0, 0, 0, 110));
    QPen pen(Qt::white, 0);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(m_rect);
}

// ---------------------------------------------------------------- Eyedropper

void EyedropperTool::mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) {
    if (b != Qt::LeftButton) return;
    QImage img = m_host.doc().composite();
    QPoint pt = p.toPoint();
    if (!img.rect().contains(pt)) return;
    QColor c = QColor::fromRgba(img.pixel(pt));
    if (c.alpha() == 0) return;
    c.setAlpha(255);
    ToolOptions& o = m_host.options();
    if (mods & Qt::AltModifier) o.bg = c;
    else o.fg = c;
    m_host.setStatus(QString("Sampled %1").arg(c.name()));
}

// ---------------------------------------------------------------- Brush / eraser

void BrushTool::mousePress(const QPointF& p, Qt::KeyboardModifiers, Qt::MouseButton b) {
    if (b != Qt::LeftButton) return;
    if (!editableActive(m_host)) return;
    m_host.beginEdit(m_eraser ? "Erase" : "Brush Stroke");
    m_dragging = true;
    m_last = p;
    m_cursor = p;
    m_hasCursor = true;
    ToolOptions& o = m_host.options();
    Layer* l = m_host.doc().active();
    Ops::strokeBrush(*l, m_host.doc().selection.get(), p, p, o.brushSize, o.fg, o.hardness, m_eraser);
    m_host.update();
}

void BrushTool::mouseMove(const QPointF& p, Qt::KeyboardModifiers) {
    m_cursor = p;
    m_hasCursor = true;
    if (!m_dragging) {
        m_host.update();
        return;
    }
    ToolOptions& o = m_host.options();
    Layer* l = m_host.doc().active();
    if (l) Ops::strokeBrush(*l, m_host.doc().selection.get(), m_last, p, o.brushSize, o.fg, o.hardness, m_eraser);
    m_last = p;
    m_host.update();
}

void BrushTool::mouseRelease(const QPointF&, Qt::KeyboardModifiers) {
    if (!m_dragging) return;
    m_dragging = false;
    m_host.commitEdit();
    m_host.update();
}

void BrushTool::drawOverlay(QPainter& p, double zoom) {
    Q_UNUSED(zoom)
    if (!m_hasCursor) return;
    QPen pen(Qt::black, 0);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    double r = m_host.options().brushSize / 2.0;
    p.drawEllipse(m_cursor, r, r);
}

// ---------------------------------------------------------------- Bucket

void BucketTool::mousePress(const QPointF& p, Qt::KeyboardModifiers, Qt::MouseButton b) {
    if (b != Qt::LeftButton) return;
    Layer* l = editableActive(m_host);
    if (!l) return;
    Document& d = m_host.doc();
    QPoint seed = p.toPoint() - QPoint(l->x, l->y);
    if (!l->rect().contains(p.toPoint())) return;
    const ToolOptions& o = m_host.options();
    auto mask = Ops::floodMask(l->image, seed, o.tolerance, o.contiguous);
    auto sel = Ops::layerMaskFromSelection(*l, d.selection.get());
    for (size_t i = 0; i < mask.size(); ++i) mask[i] = uint8_t(mask[i] * sel[i] / 255);
    m_host.beginEdit("Paint Bucket");
    Ops::fillWithMask(*l, mask, o.fg);
    m_host.commitEdit();
    m_host.update();
}

// ---------------------------------------------------------------- Gradient

void GradientTool::mousePress(const QPointF& p, Qt::KeyboardModifiers, Qt::MouseButton b) {
    if (b != Qt::LeftButton) return;
    m_start = m_cur = p;
    m_dragging = true;
}

void GradientTool::mouseMove(const QPointF& p, Qt::KeyboardModifiers) {
    if (!m_dragging) return;
    m_cur = p;
    m_host.update();
}

void GradientTool::mouseRelease(const QPointF& p, Qt::KeyboardModifiers) {
    if (!m_dragging) return;
    m_dragging = false;
    m_cur = p;
    Layer* l = editableActive(m_host);
    if (l && m_start != m_cur) {
        const ToolOptions& o = m_host.options();
        m_host.beginEdit("Gradient");
        Ops::fillGradient(*l, m_host.doc().selection.get(), m_start, m_cur, o.fg, o.bg);
        m_host.commitEdit();
    }
    m_host.update();
}

void GradientTool::drawOverlay(QPainter& p, double zoom) {
    Q_UNUSED(zoom)
    if (!m_dragging) return;
    QPen pen(Qt::black, 0);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.drawLine(m_start, m_cur);
}

// ---------------------------------------------------------------- Text

void TextTool::mousePress(const QPointF& p, Qt::KeyboardModifiers, Qt::MouseButton b) {
    if (b != Qt::LeftButton) return;
    Layer* l = editableActive(m_host);
    if (!l) return;
    const QString text = m_host.promptText();
    if (text.isEmpty()) return;
    const ToolOptions& o = m_host.options();
    QFont f(o.fontFamily);
    f.setPointSize(o.fontSize);
    m_host.beginEdit("Text");
    Ops::drawText(*l, m_host.doc().selection.get(), p, text, f, o.fg);
    m_host.commitEdit();
    m_host.update();
}

// ---------------------------------------------------------------- Shape

QRectF ShapeTool::rectFor(Qt::KeyboardModifiers mods) const {
    QPointF end = m_cur;
    if (mods & Qt::ShiftModifier) {
        double s = std::max(std::abs(end.x() - m_start.x()), std::abs(end.y() - m_start.y()));
        end = QPointF(m_start.x() + (end.x() < m_start.x() ? -s : s), m_start.y() + (end.y() < m_start.y() ? -s : s));
    }
    return QRectF(m_start, end).normalized();
}

void ShapeTool::mousePress(const QPointF& p, Qt::KeyboardModifiers, Qt::MouseButton b) {
    if (b != Qt::LeftButton) return;
    m_start = m_cur = p;
    m_dragging = true;
}

void ShapeTool::mouseMove(const QPointF& p, Qt::KeyboardModifiers) {
    if (!m_dragging) return;
    m_cur = p;
    m_host.update();
}

void ShapeTool::mouseRelease(const QPointF& p, Qt::KeyboardModifiers mods) {
    if (!m_dragging) return;
    m_dragging = false;
    m_cur = p;
    Layer* l = editableActive(m_host);
    QRectF r = rectFor(mods);
    if (l && r.width() >= 1 && r.height() >= 1) {
        const ToolOptions& o = m_host.options();
        m_host.beginEdit(m_ellipse ? "Ellipse" : "Rectangle");
        if (o.shapeStroke)
            Ops::drawShape(*l, m_host.doc().selection.get(), r, m_ellipse, Qt::transparent, o.fg, o.strokeWidth);
        else
            Ops::drawShape(*l, m_host.doc().selection.get(), r, m_ellipse, o.fg, Qt::transparent, 0);
        m_host.commitEdit();
    }
    m_host.update();
}

void ShapeTool::drawOverlay(QPainter& p, double zoom) {
    Q_UNUSED(zoom)
    if (!m_dragging) return;
    QPen pen(Qt::black, 0);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QRectF r = rectFor(Qt::NoModifier);
    if (m_ellipse) p.drawEllipse(r);
    else p.drawRect(r);
}

// ---------------------------------------------------------------- Zoom

void ZoomTool::mousePress(const QPointF& p, Qt::KeyboardModifiers mods, Qt::MouseButton b) {
    if (b != Qt::LeftButton) return;
    m_host.zoomAt(p, !(mods & Qt::AltModifier));
}

// ---------------------------------------------------------------- Transform

namespace {
constexpr double kPi = 3.14159265358979323846;
double angleOf(const QPointF& c, const QPointF& p) {
    return std::atan2(p.y() - c.y(), p.x() - c.x());
}
}  // namespace

void TransformTool::begin() {
    m_active = false;
    Layer* l = m_host.doc().active();
    if (!l || l->locked) return;
    m_source = l->image;
    m_srcX = l->x;
    m_srcY = l->y;
    m_box = QRectF(0, 0, l->w(), l->h());
    m_t = QTransform::fromTranslate(l->x, l->y);  // layer-local -> canvas
    m_drag = Drag::None;
    m_active = true;
    m_host.update();
}

void TransformTool::cancel() {
    m_active = false;
    m_drag = Drag::None;
    m_source = QImage();
}

QPointF TransformTool::handlePos(int idx) const {
    if (idx < 4) {
        static const int cx[4] = {0, 1, 1, 0}, cy[4] = {0, 0, 1, 1};
        return m_t.map(QPointF(m_box.left() + cx[idx] * m_box.width(), m_box.top() + cy[idx] * m_box.height()));
    }
    // Rotation handle: 24 units above the top edge (in layer pixels).
    return m_t.map(QPointF(m_box.center().x(), m_box.top() - 24));
}

void TransformTool::mousePress(const QPointF& p, Qt::KeyboardModifiers, Qt::MouseButton b) {
    if (b != Qt::LeftButton || !m_active) return;
    m_pressCanvas = p;
    m_startT = m_t;
    const double tol = 10;
    m_drag = Drag::None;
    m_corner = -1;
    if (QLineF(p, handlePos(4)).length() <= tol) {
        m_drag = Drag::Rotate;
    } else {
        for (int i = 0; i < 4; ++i)
            if (QLineF(p, handlePos(i)).length() <= tol) {
                m_drag = Drag::Corner;
                m_corner = i;
                break;
            }
    }
    if (m_drag == Drag::None && QPolygonF(m_t.map(m_box)).containsPoint(p, Qt::OddEvenFill))
        m_drag = Drag::Move;
    if (m_drag == Drag::Rotate) {
        QPointF c = m_t.map(m_box.center());
        m_startAngle = angleOf(c, p);
    }
}

void TransformTool::mouseMove(const QPointF& p, Qt::KeyboardModifiers mods) {
    if (!m_active || m_drag == Drag::None) return;
    const QPointF d = p - m_pressCanvas;
    switch (m_drag) {
    case Drag::Move:
        m_t = m_startT * QTransform::fromTranslate(d.x(), d.y());
        break;
    case Drag::Rotate: {
        QPointF c = m_startT.map(m_box.center());
        double deg = (angleOf(c, p) - m_startAngle) * 180.0 / kPi;
        QTransform r = QTransform().translate(-c.x(), -c.y()).rotate(deg).translate(c.x(), c.y());
        m_t = m_startT * r;
        break;
    }
    case Drag::Corner: {
        // Scale about the opposite corner, working in layer-local coordinates.
        QTransform inv = m_startT.inverted();
        QPointF lp = inv.map(m_pressCanvas);
        QPointF lc = inv.map(p);
        const int opp = (m_corner + 2) % 4;
        static const int cx[4] = {0, 1, 1, 0}, cy[4] = {0, 0, 1, 1};
        QPointF o(m_box.left() + cx[opp] * m_box.width(), m_box.top() + cy[opp] * m_box.height());
        double dxp = lp.x() - o.x(), dyp = lp.y() - o.y();
        double sx = std::abs(dxp) > 1e-6 ? (lc.x() - o.x()) / dxp : 1.0;
        double sy = std::abs(dyp) > 1e-6 ? (lc.y() - o.y()) / dyp : 1.0;
        if (mods & Qt::ShiftModifier) {
            double s = std::max(std::abs(sx), std::abs(sy));
            sx = sx < 0 ? -s : s;
            sy = sy < 0 ? -s : s;
        }
        sx = std::clamp(sx, -50.0, 50.0);
        sy = std::clamp(sy, -50.0, 50.0);
        if (std::abs(sx) < 0.01) sx = 0.01;
        if (std::abs(sy) < 0.01) sy = 0.01;
        QTransform s = QTransform().translate(-o.x(), -o.y()).scale(sx, sy).translate(o.x(), o.y());
        m_t = s * m_startT;
        break;
    }
    case Drag::None:
        break;
    }
    m_host.update();
}

void TransformTool::mouseRelease(const QPointF&, Qt::KeyboardModifiers) {
    m_drag = Drag::None;
}

void TransformTool::commit() {
    if (!m_active) return;
    Layer* l = m_host.doc().active();
    if (!l) {
        cancel();
        return;
    }
    m_host.beginEdit("Free Transform");
    // Layer-local (current image) coordinates -> new layer-local coordinates.
    QTransform local = m_t * QTransform::fromTranslate(-m_srcX, -m_srcY);
    l->image = m_source;
    l->x = m_srcX;
    l->y = m_srcY;
    Ops::transformLayer(*l, local);
    m_host.commitEdit();
    cancel();
    m_host.documentChanged();
    m_host.update();
}

bool TransformTool::keyPress(int key) {
    if (!m_active) return false;
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        commit();
        return true;
    }
    if (key == Qt::Key_Escape) {
        cancel();
        m_host.update();
        return true;
    }
    return false;
}

void TransformTool::drawOverlay(QPainter& p, double zoom) {
    if (!m_active) return;
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setTransform(m_t, true);
    p.drawImage(m_box.topLeft(), m_source);
    p.restore();

    QPen pen(QColor(30, 144, 255), 0);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPolygonF box(m_t.map(m_box));
    p.drawPolygon(box);
    p.drawLine(handlePos(0), handlePos(1));
    p.drawLine(handlePos(4), m_t.map(QPointF(m_box.center().x(), m_box.top())));
    // Handles are drawn at a constant screen size.
    const double hs = 6.0 / zoom;
    p.setBrush(Qt::white);
    for (int i = 0; i < 5; ++i) p.drawEllipse(handlePos(i), hs, hs);
}
