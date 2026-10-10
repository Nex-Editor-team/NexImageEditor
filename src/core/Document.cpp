#include "core/Document.h"

#include <algorithm>

Document::Document(int w, int h, Color bg, std::string n) : name(std::move(n)), m_w(w), m_h(h) {
    Layer bgLayer;
    bgLayer.img = newSurface(w, h);
    fillSurface(bgLayer.img, bg);
    bgLayer.name = "Background";
    bgLayer.background = true;
    m_layers.push_back(std::move(bgLayer));
}

void Document::setActive(int i) {
    if (i >= 0 && i < int(m_layers.size())) m_active = i;
}

Layer* Document::active() {
    return m_active < int(m_layers.size()) ? &m_layers[size_t(m_active)] : nullptr;
}
const Layer* Document::active() const {
    return m_active < int(m_layers.size()) ? &m_layers[size_t(m_active)] : nullptr;
}

int Document::addLayer(const std::string& layerName, const Surface& img, int x, int y) {
    Layer l;
    l.img = img;
    l.name = layerName;
    l.x = x;
    l.y = y;
    m_layers.insert(m_layers.begin() + m_active + 1, std::move(l));
    m_active += 1;
    return m_active;
}

void Document::removeLayer(int i) {
    if (m_layers.size() <= 1 || i < 0 || i >= int(m_layers.size())) return;
    m_layers.erase(m_layers.begin() + i);
    if (m_active >= int(m_layers.size())) m_active = int(m_layers.size()) - 1;
    else if (m_active > i) m_active -= 1;
}

void Document::moveLayer(int from, int to) {
    const int n = int(m_layers.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) return;
    Layer l = std::move(m_layers[size_t(from)]);
    m_layers.erase(m_layers.begin() + from);
    m_layers.insert(m_layers.begin() + to, std::move(l));
    m_active = to;
}

void Document::duplicateLayer(int i) {
    if (i < 0 || i >= int(m_layers.size())) return;
    Layer copy = m_layers[size_t(i)];
    copy.name += " copy";
    copy.background = false;
    m_layers.insert(m_layers.begin() + i + 1, std::move(copy));
    m_active = i + 1;
}

void Document::mergeDown(int i) {
    if (i <= 0 || i >= int(m_layers.size())) return;
    Layer& below = m_layers[size_t(i - 1)];
    const Layer& top = m_layers[size_t(i)];
    Surface out = cloneSurface(below.img);
    cairo_t* cr = cairo_create(out.get());
    cairo_set_source_surface(cr, top.img.get(), top.x - below.x, top.y - below.y);
    cairo_set_operator(cr, blendOperator(top.blend));
    cairo_paint_with_alpha(cr, top.opacity);
    cairo_destroy(cr);
    surfMarkDirty(out);
    below.img = out;
    m_layers.erase(m_layers.begin() + i);
    m_active = i - 1;
}

void Document::flattenAll() {
    Surface img = flattened();
    Layer bg;
    bg.img = img;
    bg.name = "Background";
    bg.background = true;
    m_layers.clear();
    m_layers.push_back(std::move(bg));
    m_active = 0;
}

DocState Document::snapshot() const {
    DocState s;
    s.layers = m_layers;
    s.active = m_active;
    s.selection = selection;
    s.w = m_w;
    s.h = m_h;
    return s;
}

void Document::restore(const DocState& s) {
    m_layers = s.layers;
    m_active = s.active;
    selection = s.selection;
    m_w = s.w;
    m_h = s.h;
}

Surface Document::flattened() const {
    Surface out = newSurface(m_w, m_h);
    cairo_t* cr = cairo_create(out.get());
    for (const Layer& l : m_layers) {
        if (!l.visible) continue;
        cairo_set_source_surface(cr, l.img.get(), l.x, l.y);
        cairo_set_operator(cr, blendOperator(l.blend));
        cairo_paint_with_alpha(cr, l.opacity);
    }
    cairo_destroy(cr);
    surfMarkDirty(out);
    return out;
}
