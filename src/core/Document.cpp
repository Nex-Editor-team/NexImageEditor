#include "core/Document.h"

#include "core/Pixels.h"

#include <QPainter>
#include <algorithm>

Document::Document(int w, int h, const QColor& bg, const QString& n) : name(n), m_w(w), m_h(h) {
    Layer bgLayer;
    bgLayer.image = newImage(w, h, bg);
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

Layer& Document::addLayer(const QString& layerName, const QImage& img, int x, int y) {
    Layer l;
    l.image = img;
    l.name = layerName;
    l.x = x;
    l.y = y;
    m_layers.insert(m_layers.begin() + m_active + 1, std::move(l));
    m_active += 1;
    return m_layers[size_t(m_active)];
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
    QPainter p(&below.image);
    p.setOpacity(top.opacity);
    p.setCompositionMode(blendComposition(top.blend));
    p.drawImage(top.x - below.x, top.y - below.y, top.image);
    p.end();
    m_layers.erase(m_layers.begin() + i);
    m_active = i - 1;
}

void Document::flattenAll() {
    QImage img = flattenedVisible();
    Layer bg;
    bg.image = img;
    bg.name = "Background";
    bg.background = true;
    m_layers.clear();
    m_layers.push_back(std::move(bg));
    m_active = 0;
}

DocState Document::snapshot() const {
    DocState s;
    s.layers = m_layers;
    s.activeLayer = m_active;
    s.selection = selection;
    s.canvasW = m_w;
    s.canvasH = m_h;
    return s;
}

void Document::restore(const DocState& s) {
    m_layers = s.layers;
    m_active = s.activeLayer;
    selection = s.selection;
    m_w = s.canvasW;
    m_h = s.canvasH;
}

QImage Document::flattenedVisible() const {
    QImage out = newImage(m_w, m_h, Qt::transparent);
    QPainter p(&out);
    for (const Layer& l : m_layers) {
        if (!l.visible) continue;
        p.setOpacity(l.opacity);
        p.setCompositionMode(blendComposition(l.blend));
        p.drawImage(l.x, l.y, l.image);
    }
    p.end();
    return out;
}

QImage Document::composite() const { return flattenedVisible(); }
