#include "ui/CanvasView.h"

#include "core/Adjustments.h"
#include "core/Pixels.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kMinZoom = 0.05, kMaxZoom = 32.0;
constexpr double kHandle = 5.0;  // half size of a transform handle, screen px

constexpr double kTau = 6.283185307179586;

}  // namespace

CanvasView::CanvasView(Hooks hooks, Document doc) : m_hooks(std::move(hooks)), m_doc(std::move(doc)) {
    buildWidgets();
}

CanvasView::~CanvasView() = default;

std::string CanvasView::title() const { return m_doc.name + (m_hist.isClean() ? "" : " *"); }

void CanvasView::buildWidgets() {
    m_area.set_hexpand(true);
    m_area.set_vexpand(true);
    m_area.set_focusable(true);
    m_area.set_draw_func(sigc::mem_fun(*this, &CanvasView::draw));
    m_rulerTop.set_hexpand(true);
    m_rulerTop.set_size_request(-1, 20);
    m_rulerTop.set_draw_func([this](const Cairo::RefPtr<Cairo::Context>& cr, int w, int h) {
        drawRuler(cr, w, h, true);
    });
    m_rulerLeft.set_vexpand(true);
    m_rulerLeft.set_size_request(20, -1);
    m_rulerLeft.set_draw_func([this](const Cairo::RefPtr<Cairo::Context>& cr, int w, int h) {
        drawRuler(cr, w, h, false);
    });
    m_corner.set_size_request(20, 20);
    m_corner.add_css_class("ruler-corner");

    m_root = std::make_unique<Gtk::Grid>();
    m_root->attach(m_corner, 0, 0);
    m_root->attach(m_rulerTop, 1, 0);
    m_root->attach(m_rulerLeft, 0, 1);
    m_root->attach(m_area, 1, 1);
    m_root->add_css_class("canvas-root");

    auto drag = Gtk::GestureDrag::create();
    drag->set_button(GDK_BUTTON_PRIMARY);
    Gtk::GestureDrag* dragRaw = drag.get();  // raw pointer: the controller is owned by the widget
    drag->signal_drag_begin().connect([this, dragRaw](double x, double y) {
        GdkModifierType st = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(dragRaw->gobj()));
        onDragBegin(x, y, (st & GDK_SHIFT_MASK) != 0, (st & GDK_ALT_MASK) != 0);
    });
    drag->signal_drag_update().connect([this](double dx, double dy) { onDragUpdate(dx, dy); });
    drag->signal_drag_end().connect([this](double dx, double dy) { onDragEnd(dx, dy); });
    m_area.add_controller(drag);

    auto pan = Gtk::GestureDrag::create();
    pan->set_button(GDK_BUTTON_MIDDLE);
    pan->signal_drag_begin().connect([this](double x, double y) {
        m_drag = Drag::Pan;
        m_startSX = x;
        m_startSY = y;
        m_panStartX = m_panX;
        m_panStartY = m_panY;
    });
    pan->signal_drag_update().connect([this](double dx, double dy) {
        m_panX = m_panStartX + dx;
        m_panY = m_panStartY + dy;
        redrawAll();
        if (m_hooks.viewMoved) m_hooks.viewMoved(*this);
    });
    pan->signal_drag_end().connect([this](double, double) { m_drag = Drag::None; });
    m_area.add_controller(pan);

    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_motion().connect([this](double x, double y) {
        PointD c = toCanvas(x, y);
        m_cursor = IPoint{int(std::floor(c.x)), int(std::floor(c.y))};
        if (m_hooks.viewMoved) m_hooks.viewMoved(*this);
    });
    m_area.add_controller(motion);

    auto scroll = Gtk::EventControllerScroll::create();
    scroll->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    scroll->signal_scroll().connect(sigc::mem_fun(*this, &CanvasView::onScroll), false);
    m_area.add_controller(scroll);
}

bool CanvasView::onScroll(double, double dy) {
    GdkModifierType st = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(m_area.gobj()));
    if (st & GDK_CONTROL_MASK) {
        const double ax = m_cursor.x * m_zoom + m_panX, ay = m_cursor.y * m_zoom + m_panY;
        zoomBy(dy < 0 ? 1.1 : 1 / 1.1, ax, ay);
    } else {
        m_panY -= dy * 40;
        redrawAll();
        if (m_hooks.viewMoved) m_hooks.viewMoved(*this);
    }
    return true;
}

void CanvasView::setTool(ToolId t, const ToolOptions& o) {
    if (t != m_tool) cancelOverlay();
    m_tool = t;
    m_opts = o;
    m_area.set_cursor(Gdk::Cursor::create(t == ToolId::Hand ? "grab" : "crosshair"));
}

void CanvasView::refresh() {
    m_compositeDirty = true;
    redrawAll();
    if (m_hooks.changed) m_hooks.changed(*this);
}

void CanvasView::touch() {
    m_compositeDirty = true;
    redrawAll();
}

void CanvasView::redrawAll() {
    m_area.queue_draw();
    m_rulerTop.queue_draw();
    m_rulerLeft.queue_draw();
}

void CanvasView::zoomFit() {
    const double aw = m_area.get_width(), ah = m_area.get_height();
    if (aw < 2 || ah < 2) {
        m_needsFit = true;
        return;
    }
    const double W = m_doc.width(), H = m_doc.height();
    m_zoom = std::clamp(std::min(aw / W, ah / H) * 0.9, kMinZoom, kMaxZoom);
    m_panX = (aw - W * m_zoom) / 2;
    m_panY = (ah - H * m_zoom) / 2;
    m_needsFit = false;
    redrawAll();
    if (m_hooks.viewMoved) m_hooks.viewMoved(*this);
}

void CanvasView::zoomBy(double factor, double ax, double ay) {
    const double nz = std::clamp(m_zoom * factor, kMinZoom, kMaxZoom);
    const double k = nz / m_zoom;
    m_panX = ax - (ax - m_panX) * k;
    m_panY = ay - (ay - m_panY) * k;
    m_zoom = nz;
    redrawAll();
    if (m_hooks.viewMoved) m_hooks.viewMoved(*this);
}

void CanvasView::recordEdit(const std::string& name, const DocState& before) {
    m_hist.push(name, before, m_doc.snapshot());
    refresh();
}

void CanvasView::undo() {
    if (const DocState* s = m_hist.undo()) {
        m_doc.restore(*s);
        refresh();
    }
}

void CanvasView::redo() {
    if (const DocState* s = m_hist.redo()) {
        m_doc.restore(*s);
        refresh();
    }
}

void CanvasView::jumpHistory(size_t applied) {
    m_hist.jumpTo(applied, m_doc);
    refresh();
}

void CanvasView::commitOverlay() {
    if (m_cropActive) {
        DocState b = snapshot();
        ops::cropDocument(m_doc, m_crop);
        m_cropActive = false;
        recordEdit("Crop", b);
        return;
    }
    if (m_transformActive) {
        Layer* L = m_doc.active();
        m_transformActive = false;
        if (!L) return;
        const IRect orig = L->rect();
        const double sx = m_tbox.w / orig.w, sy = m_tbox.h / orig.h;
        cairo_matrix_t M;
        cairo_matrix_init(&M, sx, 0, 0, sy, m_tbox.x - orig.x * sx, m_tbox.y - orig.y * sy);
        DocState b = snapshot();
        ops::transformLayer(*L, M);
        recordEdit("Free Transform", b);
    }
}

void CanvasView::cancelOverlay() {
    if (!hasOverlay()) return;
    m_cropActive = false;
    m_transformActive = false;
    touch();
}

SelectionPtr CanvasView::combine(const SelectionPtr& add) const {
    return combineSelections(m_doc.selection, add, m_opts.selectionMode, m_doc.width(), m_doc.height());
}

IRect CanvasView::rectFrom(PointD a, PointD b) {
    const double x0 = std::floor(std::min(a.x, b.x)), y0 = std::floor(std::min(a.y, b.y));
    const double x1 = std::ceil(std::max(a.x, b.x)), y1 = std::ceil(std::max(a.y, b.y));
    return IRect{int(x0), int(y0), int(x1 - x0), int(y1 - y0)};
}

void CanvasView::selectAll() {
    DocState b = snapshot();
    m_doc.selection = ops::selRect(IRect{0, 0, m_doc.width(), m_doc.height()}, m_doc.width(), m_doc.height());
    recordEdit("Select All", b);
}

void CanvasView::deselect() {
    if (!m_doc.selection) return;
    DocState b = snapshot();
    m_doc.selection = nullptr;
    recordEdit("Deselect", b);
}

void CanvasView::invertSelection() {
    DocState b = snapshot();
    const int W = m_doc.width(), H = m_doc.height();
    if (!m_doc.selection) {
        m_doc.selection = ops::selRect(IRect{0, 0, W, H}, W, H);
    } else {
        std::vector<uint8_t> m(size_t(W) * size_t(H));
        for (size_t i = 0; i < m.size(); ++i) m[i] = uint8_t(255 - m_doc.selection->mask[i]);
        m_doc.selection = Selection::fromMask(std::move(m), W, H);
    }
    recordEdit("Invert Selection", b);
}

void CanvasView::deleteSelection() {
    Layer* L = m_doc.active();
    if (!L || L->locked) return;
    DocState b = snapshot();
    ops::clearSelection(*L, m_doc.selection.get());
    recordEdit("Delete", b);
}

void CanvasView::fillSelection(Color c) {
    Layer* L = m_doc.active();
    if (!L || L->locked) return;
    DocState b = snapshot();
    ops::fillSelection(*L, m_doc.selection.get(), c);
    recordEdit("Fill", b);
}

const Surface& CanvasView::composite() {
    if (m_compositeDirty || !m_composite) {
        m_composite = m_doc.flattened();
        m_compositeDirty = false;
    }
    return m_composite;
}

Color CanvasView::sampleComposite(int x, int y) {
    const Surface& s = composite();
    if (x < 0 || y < 0 || x >= surfW(s) || y >= surfH(s)) return Color{0, 0, 0, 0};
    surfFlush(s);
    const uint32_t v = surfRow(s, y)[x];
    const uint32_t a = v >> 24;
    if (a == 0) return Color{0, 0, 0, 0};
    auto ch = [&](int shift) { return uint8_t(std::min<uint32_t>(255, (((v >> shift) & 0xff) * 255 + a / 2) / a)); };
    return Color{ch(16), ch(8), ch(0), uint8_t(a)};
}

// ---- interaction -----------------------------------------------------------

CanvasView::Handle CanvasView::handleAt(double sx, double sy) const {
    const PointD a = toScreen({m_tbox.x, m_tbox.y});
    const PointD b = toScreen({m_tbox.x + m_tbox.w, m_tbox.y + m_tbox.h});
    const double mx = (a.x + b.x) / 2, my = (a.y + b.y) / 2;
    struct P { double x, y; Handle h; };
    const P pts[] = {{a.x, a.y, Handle::NW}, {mx, a.y, Handle::N}, {b.x, a.y, Handle::NE}, {b.x, my, Handle::E},
                     {b.x, b.y, Handle::SE}, {mx, b.y, Handle::S}, {a.x, b.y, Handle::SW}, {a.x, my, Handle::W}};
    for (const P& p : pts)
        if (std::abs(sx - p.x) <= kHandle + 3 && std::abs(sy - p.y) <= kHandle + 3) return p.h;
    if (sx >= a.x && sx <= b.x && sy >= a.y && sy <= b.y) return Handle::Move;
    return Handle::None;
}

void CanvasView::onDragBegin(double x, double y, bool shift, bool alt) {
    m_shift = shift;
    m_alt = alt;
    m_startSX = x;
    m_startSY = y;
    const PointD c = toCanvas(x, y);
    m_last = c;
    m_before = snapshot();
    m_drag = Drag::None;
    m_hasPreview = false;
    Layer* L = m_doc.active();
    const bool canPaint = L && !L->locked;

    if (m_spaceHeld || m_tool == ToolId::Hand) {
        m_drag = Drag::Pan;
        m_panStartX = m_panX;
        m_panStartY = m_panY;
        return;
    }
    switch (m_tool) {
    case ToolId::Brush:
    case ToolId::Eraser:
        if (!canPaint) return;
        m_drag = Drag::Brush;
        ops::brushStroke(*L, m_doc.selection.get(), c, c, m_opts.brushSize, m_opts.fg, m_tool == ToolId::Eraser);
        touch();
        break;
    case ToolId::Move:
        if (!L || L->locked) return;
        m_drag = Drag::Move;
        m_origLayerX = L->x;
        m_origLayerY = L->y;
        break;
    case ToolId::RectMarquee:
    case ToolId::EllipseMarquee:
    case ToolId::Rectangle:
    case ToolId::Ellipse:
    case ToolId::Gradient:
        m_drag = (m_tool == ToolId::Gradient) ? Drag::Gradient
                 : (m_tool == ToolId::Rectangle || m_tool == ToolId::Ellipse) ? Drag::Shape
                                                                              : Drag::Marquee;
        break;
    case ToolId::Lasso:
        m_drag = Drag::Lasso;
        m_lasso = {IPoint{int(std::lround(c.x)), int(std::lround(c.y))}};
        break;
    case ToolId::Crop:
        m_drag = Drag::Crop;
        m_cropActive = false;
        break;
    case ToolId::Transform:
        if (!m_transformActive) {
            if (!L) return;
            const IRect r = L->rect();
            m_tbox = RectD{double(r.x), double(r.y), double(r.w), double(r.h)};
            m_transformActive = true;
        }
        m_handle = handleAt(x, y);
        if (m_handle == Handle::None) return;
        m_drag = Drag::Transform;
        m_startBox = RectD{m_tbox.x, m_tbox.y, m_tbox.w, m_tbox.h};
        break;
    case ToolId::Bucket:
    case ToolId::MagicWand:
    case ToolId::Eyedropper:
    case ToolId::Text:
    case ToolId::Zoom:
        m_drag = Drag::Click;
        break;
    default:
        break;
    }
    if (m_drag == Drag::Marquee || m_drag == Drag::Shape) {
        m_hasPreview = true;
        m_preview = IRect{};
    }
    if (m_tool == ToolId::Gradient && m_drag == Drag::Gradient) touch();
}

void CanvasView::onDragUpdate(double dx, double dy) {
    const double x = m_startSX + dx, y = m_startSY + dy;
    const PointD c = toCanvas(x, y);
    Layer* L = m_doc.active();
    switch (m_drag) {
    case Drag::Brush:
        if (L) ops::brushStroke(*L, m_doc.selection.get(), m_last, c, m_opts.brushSize, m_opts.fg,
                                m_tool == ToolId::Eraser);
        m_last = c;
        touch();
        break;
    case Drag::Move:
        if (!L) break;
        if (!m_doc.selection) {
            L->x = m_origLayerX + int(std::lround(dx / m_zoom));
            L->y = m_origLayerY + int(std::lround(dy / m_zoom));
            touch();
        }
        break;
    case Drag::Marquee:
    case Drag::Shape:
        m_preview = rectFrom(toCanvas(m_startSX, m_startSY), c);
        touch();
        break;
    case Drag::Lasso: {
        IPoint p{int(std::lround(c.x)), int(std::lround(c.y))};
        if (m_lasso.empty() || m_lasso.back().x != p.x || m_lasso.back().y != p.y) m_lasso.push_back(p);
        touch();
        break;
    }
    case Drag::Gradient:
        m_last = c;
        touch();
        break;
    case Drag::Crop:
        m_crop = rectFrom(toCanvas(m_startSX, m_startSY), c);
        m_cropActive = true;
        touch();
        break;
    case Drag::Transform: {
        const PointD s = toCanvas(m_startSX, m_startSY);
        const double ddx = c.x - s.x, ddy = c.y - s.y;
        double Lx = m_startBox.x, Rx = m_startBox.x + m_startBox.w;
        double Ty = m_startBox.y, By = m_startBox.y + m_startBox.h;
        switch (m_handle) {
        case Handle::Move:
            m_tbox = RectD{m_startBox.x + ddx, m_startBox.y + ddy, m_startBox.w, m_startBox.h};
            touch();
            return;
        case Handle::NW: Lx = c.x; Ty = c.y; break;
        case Handle::N: Ty = c.y; break;
        case Handle::NE: Rx = c.x; Ty = c.y; break;
        case Handle::E: Rx = c.x; break;
        case Handle::SE: Rx = c.x; By = c.y; break;
        case Handle::S: By = c.y; break;
        case Handle::SW: Lx = c.x; By = c.y; break;
        case Handle::W: Lx = c.x; break;
        default: break;
        }
        if (Rx - Lx < 1) Rx = Lx + 1;
        if (By - Ty < 1) By = Ty + 1;
        m_tbox = RectD{Lx, Ty, Rx - Lx, By - Ty};
        touch();
        break;
    }
    case Drag::Pan:
        m_panX = m_panStartX + dx;
        m_panY = m_panStartY + dy;
        redrawAll();
        if (m_hooks.viewMoved) m_hooks.viewMoved(*this);
        break;
    default:
        break;
    }
}

void CanvasView::onDragEnd(double dx, double dy) {
    const double x = m_startSX + dx, y = m_startSY + dy;
    const PointD c = toCanvas(x, y);
    const bool moved = std::abs(dx) > 2 || std::abs(dy) > 2;
    const Drag d = m_drag;
    m_drag = Drag::None;
    m_hasPreview = false;
    Layer* L = m_doc.active();

    switch (d) {
    case Drag::Brush:
        recordEdit(m_tool == ToolId::Eraser ? "Eraser" : "Brush", m_before);
        break;
    case Drag::Move: {
        if (!L) break;
        if (m_doc.selection) {
            const int ddx = int(std::lround(dx / m_zoom)), ddy = int(std::lround(dy / m_zoom));
            if (ddx || ddy) {
                ops::moveSelectedPixels(*L, *m_doc.selection, ddx, ddy);
                const int W = m_doc.width(), H = m_doc.height();
                std::vector<uint8_t> m(size_t(W) * size_t(H), 0);
                for (int yy = 0; yy < H; ++yy)
                    for (int xx = 0; xx < W; ++xx) {
                        int sx2 = xx - ddx, sy2 = yy - ddy;
                        m[size_t(yy) * size_t(W) + size_t(xx)] = m_doc.selection->at(sx2, sy2);
                    }
                m_doc.selection = Selection::fromMask(std::move(m), W, H);
            }
        }
        if (moved) recordEdit("Move", m_before);
        break;
    }
    case Drag::Marquee: {
        IRect r = rectFrom(toCanvas(m_startSX, m_startSY), c);
        const bool tiny = r.w <= 1 && r.h <= 1;
        SelectionPtr sel;
        if (!tiny) {
            sel = m_tool == ToolId::EllipseMarquee ? ops::selEllipse(r, m_doc.width(), m_doc.height())
                                                   : ops::selRect(r, m_doc.width(), m_doc.height());
        }
        m_doc.selection = (tiny && m_opts.selectionMode == SelOp::Replace) ? nullptr : combine(sel);
        recordEdit("Marquee", m_before);
        break;
    }
    case Drag::Lasso: {
        std::vector<IPoint> pts = m_lasso;
        m_lasso.clear();
        m_doc.selection = combine(ops::selPolygon(pts, m_doc.width(), m_doc.height()));
        recordEdit("Lasso", m_before);
        break;
    }
    case Drag::Shape: {
        if (!L || L->locked) break;
        IRect r = rectFrom(toCanvas(m_startSX, m_startSY), c);
        if (r.w < 1 || r.h < 1) break;
        ops::drawShape(*L, m_doc.selection.get(), r, m_tool == ToolId::Ellipse, m_opts.shapeFill, m_opts.fg,
                       m_opts.shapeStroke, m_opts.fg, m_opts.strokeWidth);
        recordEdit(m_tool == ToolId::Ellipse ? "Ellipse" : "Rectangle", m_before);
        break;
    }
    case Drag::Gradient:
        if (L && !L->locked && moved) {
            ops::gradientFill(*L, m_doc.selection.get(), toCanvas(m_startSX, m_startSY), c, m_opts.fg, m_opts.bg);
            recordEdit("Gradient", m_before);
        }
        touch();
        break;
    case Drag::Crop:
        if (m_crop.w < 1 || m_crop.h < 1) m_cropActive = false;
        touch();
        break;
    case Drag::Transform:
        touch();
        break;
    case Drag::Click:
        clickTool(c, PointD{x, y}, m_shift);
        break;
    default:
        break;
    }
    if (m_hooks.viewMoved) m_hooks.viewMoved(*this);
}

void CanvasView::clickTool(PointD c, PointD screen, bool shift) {
    Layer* L = m_doc.active();
    const IPoint ip{int(std::floor(c.x)), int(std::floor(c.y))};
    switch (m_tool) {
    case ToolId::Bucket:
        if (!L || L->locked) return;
        m_before = snapshot();
        ops::bucketFill(*L, m_doc.selection.get(), ip, m_opts.fg, m_opts.tolerance, m_opts.contiguous);
        recordEdit("Bucket Fill", m_before);
        break;
    case ToolId::MagicWand: {
        if (ip.x < 0 || ip.y < 0 || ip.x >= m_doc.width() || ip.y >= m_doc.height()) return;
        SelectionPtr sel = ops::selMagicWand(composite(), ip, m_opts.tolerance, m_opts.contiguous);
        DocState b = snapshot();
        m_doc.selection = combine(sel);
        recordEdit("Magic Wand", b);
        break;
    }
    case ToolId::Eyedropper:
        if (m_hooks.picked) m_hooks.picked(sampleComposite(ip.x, ip.y));
        break;
    case ToolId::Text:
        if (!L || L->locked || m_opts.text.empty()) return;
        m_before = snapshot();
        ops::drawText(*L, m_doc.selection.get(), ip, m_opts.text, m_opts.fontFamily, m_opts.fontSize, m_opts.fg);
        recordEdit("Text", m_before);
        break;
    case ToolId::Zoom:
        zoomBy(shift ? 1 / 1.25 : 1.25, screen.x, screen.y);
        break;
    default:
        break;
    }
}

// ---- drawing ---------------------------------------------------------------

void CanvasView::draw(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h) {
    if (m_needsFit && w > 1 && h > 1) {
        // Fit without queueing another draw from inside the draw callback.
        m_zoom = std::clamp(std::min(w / double(m_doc.width()), h / double(m_doc.height())) * 0.9, kMinZoom, kMaxZoom);
        m_panX = (w - m_doc.width() * m_zoom) / 2;
        m_panY = (h - m_doc.height() * m_zoom) / 2;
        m_needsFit = false;
    }
    cairo_t* c = cr->cobj();
    cairo_set_source_rgb(c, 0.19, 0.19, 0.20);
    cairo_paint(c);

    const double cw = m_doc.width() * m_zoom, ch = m_doc.height() * m_zoom;
    // Transparency checkerboard (only the visible part of the canvas).
    const double vx0 = std::max(0.0, m_panX), vy0 = std::max(0.0, m_panY);
    const double vx1 = std::min<double>(w, m_panX + cw), vy1 = std::min<double>(h, m_panY + ch);
    if (vx1 > vx0 && vy1 > vy0) {
        cairo_set_source_rgb(c, 0.80, 0.80, 0.80);
        cairo_rectangle(c, vx0, vy0, vx1 - vx0, vy1 - vy0);
        cairo_fill(c);
        cairo_set_source_rgb(c, 0.60, 0.60, 0.60);
        const double s = 8;
        const double sx0 = std::floor(vx0 / s) * s, sy0 = std::floor(vy0 / s) * s;
        for (double yy = sy0; yy < vy1; yy += s)
            for (double xx = sx0; xx < vx1; xx += s)
                if (((int(xx / s) + int(yy / s)) & 1) && xx + s > vx0 && yy + s > vy0) {
                    cairo_rectangle(c, std::max(xx, vx0), std::max(yy, vy0), s, s);
                }
        cairo_fill(c);

        // Composited image.
        const Surface& img = composite();
        cairo_save(c);
        cairo_translate(c, m_panX, m_panY);
        cairo_scale(c, m_zoom, m_zoom);
        cairo_set_source_surface(c, img.get(), 0, 0);
        cairo_pattern_set_filter(cairo_get_source(c),
                                 m_zoom >= 1 ? CAIRO_FILTER_NEAREST : CAIRO_FILTER_GOOD);
        cairo_rectangle(c, 0, 0, m_doc.width(), m_doc.height());
        cairo_fill(c);
        cairo_restore(c);
    }
    cairo_set_source_rgba(c, 0, 0, 0, 0.6);
    cairo_set_line_width(c, 1);
    cairo_rectangle(c, std::floor(m_panX) + 0.5, std::floor(m_panY) + 0.5, cw, ch);
    cairo_stroke(c);

    drawOverlays(c);
}

void CanvasView::drawOverlays(cairo_t* c) {
    // Selection outline (marching ants: dark dashes over light dashes).
    if (m_doc.selection) {
        cairo_save(c);
        for (const auto& e : m_doc.selection->edges) {
            PointD a = toScreen({double(e.first.x), double(e.first.y)});
            PointD b = toScreen({double(e.second.x), double(e.second.y)});
            cairo_move_to(c, std::round(a.x) + 0.5, std::round(a.y) + 0.5);
            cairo_line_to(c, std::round(b.x) + 0.5, std::round(b.y) + 0.5);
        }
        const double dash[] = {4, 4};
        cairo_set_line_width(c, 1);
        cairo_set_source_rgb(c, 1, 1, 1);
        cairo_set_dash(c, dash, 2, 0);
        cairo_stroke_preserve(c);
        cairo_set_source_rgb(c, 0, 0, 0);
        cairo_set_dash(c, dash, 2, 4);
        cairo_stroke(c);
        cairo_restore(c);
    }

    cairo_save(c);
    const double dash[] = {5, 4};
    cairo_set_dash(c, dash, 2, 0);
    cairo_set_line_width(c, 1);
    cairo_set_source_rgb(c, 0.1, 0.5, 1.0);
    if (m_hasPreview && (m_drag == Drag::Marquee || m_drag == Drag::Shape)) {
        PointD a = toScreen({double(m_preview.x), double(m_preview.y)});
        PointD b = toScreen({double(m_preview.right()), double(m_preview.bottom())});
        const bool ellipse = m_tool == ToolId::EllipseMarquee || m_tool == ToolId::Ellipse;
        if (ellipse) {
            cairo_save(c);
            cairo_translate(c, (a.x + b.x) / 2, (a.y + b.y) / 2);
            cairo_scale(c, std::abs(b.x - a.x) / 2, std::abs(b.y - a.y) / 2);
            cairo_arc(c, 0, 0, 1, 0, kTau);
            cairo_restore(c);
        } else {
            cairo_rectangle(c, a.x, a.y, b.x - a.x, b.y - a.y);
        }
        cairo_stroke(c);
    }
    if (m_drag == Drag::Lasso && m_lasso.size() > 1) {
        PointD p0 = toScreen({double(m_lasso[0].x), double(m_lasso[0].y)});
        cairo_move_to(c, p0.x, p0.y);
        for (const IPoint& p : m_lasso) {
            PointD q = toScreen({double(p.x), double(p.y)});
            cairo_line_to(c, q.x, q.y);
        }
        cairo_stroke(c);
    }
    if (m_drag == Drag::Gradient) {
        PointD a = toScreen(toCanvas(m_startSX, m_startSY));
        PointD b = toScreen(m_last);
        cairo_move_to(c, a.x, a.y);
        cairo_line_to(c, b.x, b.y);
        cairo_stroke(c);
    }
    cairo_restore(c);

    if (m_cropActive) {
        const PointD a = toScreen({double(m_crop.x), double(m_crop.y)});
        const PointD b = toScreen({double(m_crop.right()), double(m_crop.bottom())});
        cairo_save(c);
        cairo_rectangle(c, -1e5, -1e5, 2e5, 2e5);  // dim everything outside the crop
        cairo_rectangle(c, a.x, a.y, b.x - a.x, b.y - a.y);
        cairo_set_fill_rule(c, CAIRO_FILL_RULE_EVEN_ODD);
        cairo_set_source_rgba(c, 0, 0, 0, 0.5);
        cairo_fill(c);
        cairo_set_source_rgb(c, 1, 1, 1);
        cairo_set_line_width(c, 1);
        cairo_rectangle(c, a.x, a.y, b.x - a.x, b.y - a.y);
        cairo_stroke(c);
        cairo_restore(c);
    }

    if (m_transformActive) {
        const PointD a = toScreen({m_tbox.x, m_tbox.y});
        const PointD b = toScreen({m_tbox.x + m_tbox.w, m_tbox.y + m_tbox.h});
        cairo_save(c);
        cairo_set_source_rgb(c, 0.1, 0.5, 1.0);
        cairo_set_line_width(c, 1);
        cairo_rectangle(c, a.x + 0.5, a.y + 0.5, b.x - a.x, b.y - a.y);
        cairo_stroke(c);
        const double mx = (a.x + b.x) / 2, my = (a.y + b.y) / 2;
        const PointD hs[] = {a, {mx, a.y}, {b.x, a.y}, {b.x, my}, b, {mx, b.y}, {a.x, b.y}, {a.x, my}};
        for (const PointD& p : hs) {
            cairo_rectangle(c, p.x - kHandle, p.y - kHandle, 2 * kHandle, 2 * kHandle);
        }
        cairo_set_source_rgb(c, 1, 1, 1);
        cairo_fill_preserve(c);
        cairo_set_source_rgb(c, 0.1, 0.5, 1.0);
        cairo_stroke(c);
        cairo_restore(c);
    }
}

void CanvasView::drawRuler(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h, bool horizontal) {
    cairo_t* c = cr->cobj();
    cairo_set_source_rgb(c, 0.25, 0.25, 0.26);
    cairo_paint(c);
    static const int steps[] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000};
    int unit = steps[0];
    for (int s : steps) {
        unit = s;
        if (s * m_zoom >= 60) break;
    }
    const double origin = horizontal ? m_panX : m_panY;
    const double extent = horizontal ? w : h;
    const int first = int(std::floor((0 - origin) / m_zoom / unit)) * unit;
    const int last = int(std::ceil((extent - origin) / m_zoom / unit)) * unit;
    cairo_set_source_rgb(c, 0.8, 0.8, 0.8);
    cairo_set_line_width(c, 1);
    cairo_select_font_face(c, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(c, 9);
    for (int t = first; t <= last; t += unit) {
        const double p = std::round(origin + t * m_zoom) + 0.5;
        if (horizontal) {
            cairo_move_to(c, p, h - 8);
            cairo_line_to(c, p, h);
        } else {
            cairo_move_to(c, w - 8, p);
            cairo_line_to(c, w, p);
        }
        cairo_stroke(c);
        // Labels: the path was consumed by the stroke, so start a new one before the text.
        if (horizontal) cairo_move_to(c, p + 2, h - 9);
        else cairo_move_to(c, 1, p - 2);
        cairo_text_path(c, std::to_string(t).c_str());
        cairo_fill(c);
    }
    // Cursor marker.
    cairo_set_source_rgb(c, 1, 0.4, 0.2);
    const double cur = horizontal ? m_panX + (m_cursor.x + 0.5) * m_zoom : m_panY + (m_cursor.y + 0.5) * m_zoom;
    if (horizontal) {
        cairo_move_to(c, cur, 0);
        cairo_line_to(c, cur, 4);
    } else {
        cairo_move_to(c, 0, cur);
        cairo_line_to(c, 4, cur);
    }
    cairo_stroke(c);
}
