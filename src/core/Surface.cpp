#include "core/Surface.h"

#include <algorithm>

Surface wrapSurface(cairo_surface_t* s) {
    return Surface(s, [](cairo_surface_t* p) { cairo_surface_destroy(p); });
}

Surface newSurface(int w, int h) {
    Surface s = wrapSurface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, std::max(1, w), std::max(1, h)));
    cairo_t* cr = cairo_create(s.get());
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_destroy(cr);
    surfMarkDirty(s);
    return s;
}

Surface cloneSurface(const Surface& src) {
    Surface s = newSurface(surfW(src), surfH(src));
    cairo_t* cr = cairo_create(s.get());
    cairo_set_source_surface(cr, src.get(), 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_destroy(cr);
    surfMarkDirty(s);
    return s;
}

int surfW(const Surface& s) { return cairo_image_surface_get_width(s.get()); }
int surfH(const Surface& s) { return cairo_image_surface_get_height(s.get()); }

void surfFlush(const Surface& s) { cairo_surface_flush(s.get()); }
void surfMarkDirty(const Surface& s) { cairo_surface_mark_dirty(s.get()); }

uint32_t* surfRow(const Surface& s, int y) {
    auto* data = cairo_image_surface_get_data(s.get());
    int stride = cairo_image_surface_get_stride(s.get());
    return reinterpret_cast<uint32_t*>(data + size_t(y) * size_t(stride));
}

void fillSurface(const Surface& s, Color c) {
    cairo_t* cr = cairo_create(s.get());
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0);
    cairo_paint(cr);
    cairo_destroy(cr);
    surfMarkDirty(s);
}
