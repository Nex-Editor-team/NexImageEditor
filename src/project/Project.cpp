#include "Project.h"
#include <QPainter>
#include <algorithm>

// Ordering convention: m_layers[0] is the BOTTOM of the stack,
// m_layers[last] is the TOP (drawn last / on top). The LayersPanel
// displays the reverse (top-most first) since that's how users expect
// a layers list to read.

Project::Project(QSize canvasSize) : m_canvasSize(canvasSize) {}

Layer* Project::layerAt(int index) const {
    if (index < 0 || index >= layerCount()) return nullptr;
    return m_layers[index].get();
}

int Project::indexOf(const Layer* layer) const {
    for (size_t i = 0; i < m_layers.size(); ++i)
        if (m_layers[i].get() == layer) return static_cast<int>(i);
    return -1;
}

Layer* Project::insertLayer(std::unique_ptr<Layer> layer, int index) {
    Layer* raw = layer.get();
    if (index < 0 || index > layerCount())
        m_layers.push_back(std::move(layer));
    else
        m_layers.insert(m_layers.begin() + index, std::move(layer));
    return raw;
}

std::unique_ptr<Layer> Project::takeLayer(int index) {
    if (index < 0 || index >= layerCount()) return nullptr;
    auto it = m_layers.begin() + index;
    std::unique_ptr<Layer> out = std::move(*it);
    m_layers.erase(it);
    return out;
}

void Project::moveLayer(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= layerCount()) return;
    toIndex = std::clamp(toIndex, 0, layerCount() - 1);
    if (fromIndex == toIndex) return;
    auto layer = std::move(m_layers[fromIndex]);
    m_layers.erase(m_layers.begin() + fromIndex);
    m_layers.insert(m_layers.begin() + toIndex, std::move(layer));
}

void Project::render(QPainter& painter) const {
    for (const auto& layer : m_layers)
        layer->render(painter);
}

ImageBuffer Project::flatten() const {
    ImageBuffer buf(m_canvasSize.width(), m_canvasSize.height(), Qt::transparent);
    QPainter painter(&buf.image());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    render(painter);
    painter.end();
    return buf;
}
