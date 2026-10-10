#pragma once
#include <QSize>
#include <QString>
#include <vector>
#include <memory>
#include "Layer.h"
#include "../engine/ImageBuffer.h"

// Holds the layer stack and canvas size for one open document.
// Layer ownership lives here; GUI code only ever holds raw Layer*
// (borrowed pointers) or an index into the stack.
class Project {
public:
    Project() = default;
    explicit Project(QSize canvasSize);

    QSize canvasSize() const { return m_canvasSize; }
    void setCanvasSize(QSize size) { m_canvasSize = size; }

    QString filePath; // empty until first save

    const std::vector<std::unique_ptr<Layer>>& layers() const { return m_layers; }

    int layerCount() const { return static_cast<int>(m_layers.size()); }
    Layer* layerAt(int index) const;
    int indexOf(const Layer* layer) const;

    // Takes ownership; returns a borrowed pointer to the inserted layer.
    Layer* insertLayer(std::unique_ptr<Layer> layer, int index = -1);
    std::unique_ptr<Layer> takeLayer(int index); // removes + returns ownership (for undo)
    void moveLayer(int fromIndex, int toIndex);

    // Renders all visible layers, bottom to top, onto `painter`.
    void render(QPainter& painter) const;

    // Flattens the whole stack into a single pixel buffer of canvasSize.
    ImageBuffer flatten() const;

private:
    QSize m_canvasSize{1920, 1080};
    std::vector<std::unique_ptr<Layer>> m_layers; // back = bottom, front = top... see .cpp for ordering note
};
