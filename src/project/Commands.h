#pragma once
#include <QUndoCommand>
#include <QUuid>
#include <QString>
#include <memory>
#include <functional>
#include "Project.h"
#include "Layer.h"

// All commands operate on a Project* by Layer UUID (not raw pointer)
// so that undoing an add/remove that swaps pointers around still finds
// the right layer.

class TransformLayerCommand : public QUndoCommand {
public:
    TransformLayerCommand(Project* project, QUuid layerId,
                           Transform::FreeTransformState before,
                           Transform::FreeTransformState after,
                           QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;
    // Canvas commits once per completed drag: keep each gesture independently undoable.

private:
    Project* m_project;
    QUuid m_layerId;
    Transform::FreeTransformState m_before;
    Transform::FreeTransformState m_after;
    Layer* findLayer() const;
};

class AddLayerCommand : public QUndoCommand {
public:
    AddLayerCommand(Project* project, std::unique_ptr<Layer> layer, int index = -1, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Project* m_project;
    std::unique_ptr<Layer> m_layer; // owned while removed from project
    int m_index;
    QUuid m_layerId;
};

class RemoveLayerCommand : public QUndoCommand {
public:
    RemoveLayerCommand(Project* project, int index, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Project* m_project;
    std::unique_ptr<Layer> m_layer; // owned while removed from project
    int m_index;
};

// Generic "set one simple property and remember the old value" command,
// used for opacity/blend-mode/visibility toggles from the Properties panel.
template <typename T>
class SetLayerPropertyCommand : public QUndoCommand {
public:
    using Getter = std::function<T(Layer*)>;
    using Setter = std::function<void(Layer*, T)>;

    SetLayerPropertyCommand(Project* project, QUuid layerId, T before, T after,
                             Getter, Setter setter, QString text)
        : QUndoCommand(text), m_project(project), m_layerId(layerId),
          m_before(before), m_after(after), m_setter(std::move(setter)) {}

    void undo() override { apply(m_before); }
    void redo() override { apply(m_after); }

private:
    void apply(const T& value) {
        int idx = -1;
        for (int i = 0; i < m_project->layerCount(); ++i)
            if (m_project->layerAt(i)->id() == m_layerId) { idx = i; break; }
        if (idx < 0) return;
        m_setter(m_project->layerAt(idx), value);
    }

    Project* m_project;
    QUuid m_layerId;
    T m_before;
    T m_after;
    Setter m_setter;
};

class SetToneAdjustmentsCommand : public QUndoCommand {
public:
    SetToneAdjustmentsCommand(Project* project, QUuid layerId,
                              ToneParameters before, ToneParameters after,
                              QString text = "Change Photo Adjustments",
                              QUndoCommand* parent = nullptr)
        : QUndoCommand(std::move(text), parent), m_project(project), m_layerId(layerId),
          m_before(before), m_after(after) {}

    void undo() override { apply(m_before); }
    void redo() override { apply(m_after); }

    int id() const override { return 1002; }
    bool mergeWith(const QUndoCommand* other) override {
        if (other->id() != id()) return false;
        const auto* next = static_cast<const SetToneAdjustmentsCommand*>(other);
        if (next->m_project != m_project || next->m_layerId != m_layerId
            || next->text() != text()) return false;
        m_after = next->m_after;
        return true;
    }

private:
    void apply(const ToneParameters& val) {
        for (int i = 0; i < m_project->layerCount(); ++i) {
            if (m_project->layerAt(i)->id() == m_layerId) {
                m_project->layerAt(i)->adjustments = val;
                m_project->layerAt(i)->applyAdjustments();
                break;
            }
        }
    }

    Project* m_project;
    QUuid m_layerId;
    ToneParameters m_before;
    ToneParameters m_after;
};

// Stack ordering is restored exactly by undo/redo.
class MoveLayerCommand : public QUndoCommand {
public:
    MoveLayerCommand(Project* project, int from, int to)
        : QUndoCommand("Reorder Layer"), m_project(project), m_from(from), m_to(to) {}
    void undo() override { m_project->moveLayer(m_to, m_from); }
    void redo() override { m_project->moveLayer(m_from, m_to); }
private:
    Project* m_project;
    int m_from;
    int m_to;
};

class ResizeCanvasCommand : public QUndoCommand {
public:
    ResizeCanvasCommand(Project* project, QSize size, bool centered);
    void undo() override;
    void redo() override;
private:
    Project* m_project;
    QSize m_before, m_after;
    QPointF m_offset;
    void apply(QSize size, QPointF offset);
};

// Cropping changes document bounds without destroying off-canvas layer pixels.
class CropCanvasCommand : public QUndoCommand {
public:
    CropCanvasCommand(Project* project, QRect crop);
    void undo() override;
    void redo() override;
private:
    Project* m_project;
    QSize m_before;
    QRect m_crop;
    std::vector<std::pair<QUuid, QPointF>> m_centers;
    void apply(bool cropped);
};
