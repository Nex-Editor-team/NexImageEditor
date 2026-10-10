#include "core/Ops.h"

#include "core/Pixels.h"

#include <pango/pangocairo.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>

namespace ops {
namespace {

constexpr double kPi = 3.14159265358979323846;

// Canvas-sized A8 mask of the selection, positioned so that canvas (0,0) is at the origin.
Surface selMaskSurface(const Selection& s) {
    cairo_surface_t* a8 = cairo_image_surface_create(CAIRO_FORMAT_A8, s.w, s.h);
    cairo_surface_flush(a8);
    const int stride = cairo_image_surface_get_stride(a8);
    unsigned char* data = cairo_image_surface_get_data(a8);
    for (int y = 0; y < s.h; ++y)
        std::memcpy(data + size_t(y) * size_t(stride), &s.mask[size_t(y) * size_t(s.w)], size_t(s.w));
    cairo_surface_mark_dirty(a8);
    return wrapSurface(a8);
}

// Runs `draw` into an offscreen group in layer-local coordinates, then composites the group onto the
// layer through the selection. Erase uses DEST_OUT, so `draw` should paint opaque coverage for it.
void compose(Layer& L, const Selection* sel, bool erase, const std::function<void(cairo_t*)>& draw) {
    Surface& img = L.writable();
    cairo_t* cr = cairo_create(img.get());
    cairo_push_group(cr);
    draw(cr);
    cairo_pop_group_to_source(cr);
    if (erase) cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OUT);
    else cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    if (sel) {
        Surface m = selMaskSurface(*sel);
        cairo_mask_surface(cr, m.get(), -L.x, -L.y);
    } else {
        cairo_paint(cr);
    }
    cairo_destroy(cr);
    surfMarkDirty(img);
}

void setColor(cairo_t* cr, Color c) {
    cairo_set_source_rgba(cr, c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0);
}

// Flood fill / colour match over non-premultiplied pixels. Returns a w*h 0/255 mask.
std::vector<uint8_t> floodMask(const Pixels& p, IPoint seed, int tol, bool contiguous) {
    std::vector<uint8_t> out(size_t(p.w) * size_t(p.h), 0);
    if (seed.x < 0 || seed.y < 0 || seed.x >= p.w || seed.y >= p.h) return out;
    const uint8_t* t = p.px(seed.x, seed.y);
    auto match = [&](int x, int y) {
        const uint8_t* q = p.px(x, y);
        for (int k = 0; k < 4; ++k)
            if (std::abs(int(q[k]) - int(t[k])) > tol) return false;
        return true;
    };
    if (!contiguous) {
        for (int y = 0; y < p.h; ++y)
            for (int x = 0; x < p.w; ++x)
                if (match(x, y)) out[size_t(y) * size_t(p.w) + size_t(x)] = 255;
        return out;
    }
    std::deque<IPoint> q;
    q.push_back(seed);
    out[size_t(seed.y) * size_t(p.w) + size_t(seed.x)] = 255;
    while (!q.empty()) {
        IPoint c = q.front();
        q.pop_front();
        const IPoint n[4] = {{c.x + 1, c.y}, {c.x - 1, c.y}, {c.x, c.y + 1}, {c.x, c.y - 1}};
        for (const IPoint& v : n) {
            if (v.x < 0 || v.y < 0 || v.x >= p.w || v.y >= p.h) continue;
            size_t idx = size_t(v.y) * size_t(p.w) + size_t(v.x);
            if (out[idx] || !match(v.x, v.y)) continue;
            out[idx] = 255;
            q.push_back(v);
        }
    }
    return out;
}

Surface maskFromCairo(int w, int h, const std::function<void(cairo_t*)>& draw) {
    Surface s = wrapSurface(cairo_image_surface_create(CAIRO_FORMAT_A8, w, h));
    cairo_t* cr = cairo_create(s.get());
    draw(cr);
    cairo_destroy(cr);
    cairo_surface_flush(s.get());
    return s;
}

std::vector<uint8_t> readA8(const Surface& s, int w, int h) {
    std::vector<uint8_t> m(size_t(w) * size_t(h));
    cairo_surface_flush(s.get());
    const int stride = cairo_image_surface_get_stride(s.get());
    const unsigned char* data = cairo_image_surface_get_data(s.get());
    for (int y = 0; y < h; ++y)
        std::memcpy(&m[size_t(y) * size_t(w)], data + size_t(y) * size_t(stride), size_t(w));
    return m;
}

}  // namespace

SelectionPtr selRect(IRect r, int w, int h) {
    Surface m = maskFromCairo(w, h, [&](cairo_t* cr) {
        cairo_rectangle(cr, r.x, r.y, r.w, r.h);
        cairo_fill(cr);
    });
    return Selection::fromMask(readA8(m, w, h), w, h);
}

SelectionPtr selEllipse(IRect r, int w, int h) {
    if (r.empty()) return nullptr;
    Surface m = maskFromCairo(w, h, [&](cairo_t* cr) {
        cairo_save(cr);
        cairo_translate(cr, r.x + r.w / 2.0, r.y + r.h / 2.0);
        cairo_scale(cr, r.w / 2.0, r.h / 2.0);
        cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
        cairo_restore(cr);
        cairo_fill(cr);
    });
    return Selection::fromMask(readA8(m, w, h), w, h);
}

SelectionPtr selPolygon(const std::vector<IPoint>& pts, int w, int h) {
    if (pts.size() < 3) return nullptr;
    Surface m = maskFromCairo(w, h, [&](cairo_t* cr) {
        cairo_move_to(cr, pts[0].x, pts[0].y);
        for (size_t i = 1; i < pts.size(); ++i) cairo_line_to(cr, pts[i].x, pts[i].y);
        cairo_close_path(cr);
        cairo_fill(cr);
    });
    return Selection::fromMask(readA8(m, w, h), w, h);
}

SelectionPtr selMagicWand(const Surface& flat, IPoint seed, int tolerance, bool contiguous) {
    Pixels p = toPixels(flat);
    auto m = floodMask(p, seed, tolerance, contiguous);
    return Selection::fromMask(std::move(m), p.w, p.h);
}

void brushStroke(Layer& L, const Selection* sel, PointD a, PointD b, double size, Color c, bool erase) {
    compose(L, sel, erase, [&](cairo_t* cr) {
        if (erase) cairo_set_source_rgba(cr, 0, 0, 0, 1);
        else setColor(cr, c);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_set_line_width(cr, size);
        cairo_move_to(cr, a.x - L.x, a.y - L.y);
        cairo_line_to(cr, b.x - L.x, b.y - L.y);
        cairo_stroke(cr);
    });
}

void bucketFill(Layer& L, const Selection* sel, IPoint canvasSeed, Color c, int tolerance, bool contiguous) {
    const IPoint local{canvasSeed.x - L.x, canvasSeed.y - L.y};
    if (local.x < 0 || local.y < 0 || local.x >= L.w() || local.y >= L.h()) return;
    Pixels p = toPixels(L.img);
    auto region = floodMask(p, local, tolerance, contiguous);
    Surface regionSurf = wrapSurface(cairo_image_surface_create(CAIRO_FORMAT_A8, p.w, p.h));
    cairo_surface_flush(regionSurf.get());
    {
        const int stride = cairo_image_surface_get_stride(regionSurf.get());
        unsigned char* data = cairo_image_surface_get_data(regionSurf.get());
        for (int y = 0; y < p.h; ++y)
            std::memcpy(data + size_t(y) * size_t(stride), &region[size_t(y) * size_t(p.w)], size_t(p.w));
        cairo_surface_mark_dirty(regionSurf.get());
    }
    compose(L, sel, false, [&](cairo_t* cr) {
        setColor(cr, c);
        cairo_mask_surface(cr, regionSurf.get(), 0, 0);
    });
}

void gradientFill(Layer& L, const Selection* sel, PointD a, PointD b, Color c1, Color c2) {
    compose(L, sel, false, [&](cairo_t* cr) {
        cairo_pattern_t* pat = cairo_pattern_create_linear(a.x - L.x, a.y - L.y, b.x - L.x, b.y - L.y);
        cairo_pattern_add_color_stop_rgba(pat, 0, c1.r / 255.0, c1.g / 255.0, c1.b / 255.0, c1.a / 255.0);
        cairo_pattern_add_color_stop_rgba(pat, 1, c2.r / 255.0, c2.g / 255.0, c2.b / 255.0, c2.a / 255.0);
        cairo_set_source(cr, pat);
        cairo_paint(cr);
        cairo_pattern_destroy(pat);
    });
}

void fillSelection(Layer& L, const Selection* sel, Color c) {
    compose(L, sel, false, [&](cairo_t* cr) {
        setColor(cr, c);
        cairo_paint(cr);
    });
}

void clearSelection(Layer& L, const Selection* sel) {
    compose(L, sel, true, [&](cairo_t* cr) {
        cairo_set_source_rgba(cr, 0, 0, 0, 1);
        cairo_paint(cr);
    });
}

void drawShape(Layer& L, const Selection* sel, IRect r, bool ellipse, bool fill, Color fillColor,
               bool stroke, Color strokeColor, double strokeWidth) {
    compose(L, sel, false, [&](cairo_t* cr) {
        auto path = [&]() {
            const double x = r.x - L.x, y = r.y - L.y;
            if (!ellipse) {
                cairo_rectangle(cr, x, y, r.w, r.h);
            } else {
                cairo_save(cr);
                cairo_translate(cr, x + r.w / 2.0, y + r.h / 2.0);
                cairo_scale(cr, r.w / 2.0, r.h / 2.0);
                cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
                cairo_restore(cr);
            }
        };
        path();
        if (fill) {
            setColor(cr, fillColor);
            if (stroke) cairo_fill_preserve(cr);
            else cairo_fill(cr);
        }
        if (stroke) {
            setColor(cr, strokeColor);
            cairo_set_line_width(cr, strokeWidth);
            cairo_stroke(cr);
        }
        cairo_new_path(cr);
    });
}

void drawText(Layer& L, const Selection* sel, IPoint at, const std::string& text, const std::string& family,
              int sizePx, Color c) {
    compose(L, sel, false, [&](cairo_t* cr) {
        PangoLayout* lay = pango_cairo_create_layout(cr);
        PangoFontDescription* fd = pango_font_description_from_string(family.c_str());
        pango_font_description_set_absolute_size(fd, sizePx * PANGO_SCALE);
        pango_layout_set_font_description(lay, fd);
        pango_layout_set_text(lay, text.c_str(), -1);
        cairo_move_to(cr, at.x - L.x, at.y - L.y);
        setColor(cr, c);
        pango_cairo_show_layout(cr, lay);
        pango_font_description_free(fd);
        g_object_unref(lay);
    });
}

void moveSelectedPixels(Layer& L, const Selection& sel, int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    Surface m = selMaskSurface(sel);
    // 1) extract the selected pixels
    Surface extracted = newSurface(L.w(), L.h());
    {
        cairo_t* cr = cairo_create(extracted.get());
        cairo_set_source_surface(cr, L.img.get(), 0, 0);
        cairo_mask_surface(cr, m.get(), -L.x, -L.y);
        cairo_destroy(cr);
        surfMarkDirty(extracted);
    }
    // 2) erase them from the layer
    {
        Surface& img = L.writable();
        cairo_t* cr = cairo_create(img.get());
        cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OUT);
        cairo_mask_surface(cr, m.get(), -L.x, -L.y);
        cairo_destroy(cr);
        surfMarkDirty(img);
    }
    // 3) paste them offset
    {
        Surface& img = L.writable();
        cairo_t* cr = cairo_create(img.get());
        cairo_set_source_surface(cr, extracted.get(), dx, dy);
        cairo_paint(cr);
        cairo_destroy(cr);
        surfMarkDirty(img);
    }
}

void transformLayer(Layer& L, const cairo_matrix_t& M) {
    const double lw = L.w(), lh = L.h();
    const double lx = L.x, ly = L.y;
    const double pts[4][2] = {{lx, ly}, {lx + lw, ly}, {lx, ly + lh}, {lx + lw, ly + lh}};
    double minx = 1e18, miny = 1e18, maxx = -1e18, maxy = -1e18;
    for (auto& p : pts) {
        double X = p[0], Y = p[1];
        cairo_matrix_transform_point(&M, &X, &Y);
        minx = std::min(minx, X);
        miny = std::min(miny, Y);
        maxx = std::max(maxx, X);
        maxy = std::max(maxy, Y);
    }
    const int ox = int(std::floor(minx)), oy = int(std::floor(miny));
    const int nw = std::max(1, int(std::ceil(maxx)) - ox), nh = std::max(1, int(std::ceil(maxy)) - oy);

    // layer-local -> canvas (translate(L.x,L.y) then M) -> new layer-local (translate(-ox,-oy))
    cairo_matrix_t T1, T2, T;
    cairo_matrix_init_translate(&T1, L.x, L.y);
    cairo_matrix_multiply(&T, &T1, &M);
    cairo_matrix_init_translate(&T2, -ox, -oy);
    cairo_matrix_multiply(&T, &T, &T2);

    Surface out = newSurface(nw, nh);
    cairo_t* cr = cairo_create(out.get());
    cairo_transform(cr, &T);
    cairo_set_source_surface(cr, L.img.get(), 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_destroy(cr);
    surfMarkDirty(out);
    L.img = out;
    L.x = ox;
    L.y = oy;
}

void cropDocument(Document& d, IRect r) {
    if (r.empty()) return;
    for (Layer& l : d.layers()) {
        l.x -= r.x;
        l.y -= r.y;
    }
    d.setSize(r.w, r.h);
    d.selection = nullptr;
}

void resizeCanvas(Document& d, int w, int h, int offsetX, int offsetY) {
    if (w < 1 || h < 1) return;
    for (Layer& l : d.layers()) {
        l.x += offsetX;
        l.y += offsetY;
    }
    d.setSize(w, h);
    d.selection = nullptr;
}

void resizeImage(Document& d, int w, int h) {
    if (w < 1 || h < 1) return;
    const double sx = double(w) / d.width(), sy = double(h) / d.height();
    for (Layer& l : d.layers()) {
        const int nw = std::max(1, int(std::lround(l.w() * sx)));
        const int nh = std::max(1, int(std::lround(l.h() * sy)));
        Surface out = newSurface(nw, nh);
        cairo_t* cr = cairo_create(out.get());
        cairo_scale(cr, double(nw) / l.w(), double(nh) / l.h());
        cairo_set_source_surface(cr, l.img.get(), 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
        cairo_paint(cr);
        cairo_destroy(cr);
        surfMarkDirty(out);
        l.img = out;
        l.x = int(std::lround(l.x * sx));
        l.y = int(std::lround(l.y * sy));
    }
    d.setSize(w, h);
    d.selection = nullptr;
}

void rotateCanvas90(Document& d, bool clockwise) {
    const double W = d.width(), H = d.height();
    cairo_matrix_t M;
    if (clockwise) cairo_matrix_init(&M, 0, 1, -1, 0, H, 0);   // (x,y) -> (H-y, x)
    else cairo_matrix_init(&M, 0, -1, 1, 0, 0, W);             // (x,y) -> (y, W-x)
    for (Layer& l : d.layers()) transformLayer(l, M);
    d.setSize(int(H), int(W));
    d.selection = nullptr;
}

void rotateCanvas180(Document& d) {
    cairo_matrix_t M;
    cairo_matrix_init(&M, -1, 0, 0, -1, d.width(), d.height());
    for (Layer& l : d.layers()) transformLayer(l, M);
    d.selection = nullptr;
}

void flipCanvas(Document& d, bool horizontal) {
    cairo_matrix_t M;
    if (horizontal) cairo_matrix_init(&M, -1, 0, 0, 1, d.width(), 0);
    else cairo_matrix_init(&M, 1, 0, 0, -1, 0, d.height());
    for (Layer& l : d.layers()) transformLayer(l, M);
    d.selection = nullptr;
}

void applyResult(Layer& L, const Surface& result, const Selection* sel) {
    if (!sel) {
        Surface& img = L.writable();
        cairo_t* cr = cairo_create(img.get());
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(cr, result.get(), 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);
        surfMarkDirty(img);
        return;
    }
    const int w = L.w(), h = L.h();
    Surface& img = L.writable();
    cairo_surface_flush(img.get());
    cairo_surface_flush(result.get());
    for (int y = 0; y < h; ++y) {
        uint32_t* dst = surfRow(img, y);
        const uint32_t* src = surfRow(result, y);
        for (int x = 0; x < w; ++x) {
            const uint32_t cov = sel->at(x + L.x, y + L.y);
            if (!cov) continue;
            if (cov == 255) {
                dst[x] = src[x];
                continue;
            }
            uint32_t out = 0;
            for (int s = 0; s < 32; s += 8) {
                uint32_t a = (dst[x] >> s) & 0xff, b = (src[x] >> s) & 0xff;
                out |= ((a * (255 - cov) + b * cov + 127) / 255) << s;
            }
            dst[x] = out;
        }
    }
    surfMarkDirty(img);
}

}  // namespace ops
