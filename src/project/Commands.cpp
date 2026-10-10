#include "Commands.h"

// ---- TransformLayerCommand ----

TransformLayerCommand::TransformLayerCommand(Project* project, QUuid layerId,
                                              Transform::FreeTransformState before,
                                              Transform::FreeTransformState after,
                                              QUndoCommand* parent)
    : QUndoCommand("Transform Layer", parent), m_project(project), m_layerId(layerId),
      m_before(before), m_after(after) {}

Layer* TransformLayerCommand::findLayer() const {
    for (int i = 0; i < m_project->layerCount(); ++i) {
        Layer* l = m_project->layerAt(i);
        if (l->id() == m_layerId) return l;
    }
    return nullptr;
}

void TransformLayerCommand::undo() {
    if (Layer* l = findLayer()) l->transform = m_before;
}

void TransformLayerCommand::redo() {
    if (Layer* l = findLayer()) l->transform = m_after;
}

// ---- AddLayerCommand ----

AddLayerCommand::AddLayerCommand(Project* project, std::unique_ptr<Layer> layer, int index, QUndoCommand* parent)
    : QUndoCommand("Add Layer", parent), m_project(project), m_layer(std::move(layer)), m_index(index) {
    m_layerId = m_layer->id();
}

void AddLayerCommand::redo() {
    m_project->insertLayer(std::move(m_layer), m_index);
}

void AddLayerCommand::undo() {
    int idx = -1;
    for (int i = 0; i < m_project->layerCount(); ++i)
        if (m_project->layerAt(i)->id() == m_layerId) { idx = i; break; }
    if (idx >= 0) m_layer = m_project->takeLayer(idx);
}

// ---- RemoveLayerCommand ----

RemoveLayerCommand::RemoveLayerCommand(Project* project, int index, QUndoCommand* parent)
    : QUndoCommand("Delete Layer", parent), m_project(project), m_index(index) {}

void RemoveLayerCommand::redo() {
    m_layer = m_project->takeLayer(m_index);
}

void RemoveLayerCommand::undo() {
    if (m_layer) m_project->insertLayer(std::move(m_layer), m_index);
}

ResizeCanvasCommand::ResizeCanvasCommand(Project* project, QSize size, bool centered)
    : QUndoCommand("Resize Canvas"), m_project(project),
      m_before(project->canvasSize()), m_after(size) {
    if (centered)
        m_offset = QPointF((size.width() - m_before.width()) / 2.0,
                           (size.height() - m_before.height()) / 2.0);
}

void ResizeCanvasCommand::apply(QSize size, QPointF offset) {
    m_project->setCanvasSize(size);
    for (const auto& layer : m_project->layers())
        layer->transform.center += offset;
}

void ResizeCanvasCommand::undo() { apply(m_before, -m_offset); }
void ResizeCanvasCommand::redo() { apply(m_after, m_offset); }

CropCanvasCommand::CropCanvasCommand(Project* project, QRect crop)
    : QUndoCommand("Crop Canvas"), m_project(project), m_before(project->canvasSize()),
      m_crop(crop.intersected(QRect(QPoint(), m_before))) {
    if (m_crop.isEmpty() || m_crop == QRect(QPoint(), m_before)) setObsolete(true);
    for (const auto& layer : project->layers())
        m_centers.emplace_back(layer->id(), layer->transform.center);
}

void CropCanvasCommand::apply(bool cropped) {
    if (isObsolete()) return;
    m_project->setCanvasSize(cropped ? m_crop.size() : m_before);
    for (const auto& layer : m_project->layers()) {
        for (const auto& entry : m_centers) {
            if (entry.first == layer->id()) {
                layer->transform.center = entry.second - (cropped ? QPointF(m_crop.topLeft()) : QPointF());
                break;
            }
        }
    }
}

void CropCanvasCommand::undo() { apply(false); }
void CropCanvasCommand::redo() { apply(true); }
