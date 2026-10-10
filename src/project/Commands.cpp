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

bool TransformLayerCommand::mergeWith(const QUndoCommand* other) {
    if (other->id() != id()) return false;
    const auto* o = static_cast<const TransformLayerCommand*>(other);
    if (o->m_layerId != m_layerId) return false;
    m_after = o->m_after; // extend this drag's end state
    return true;
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
