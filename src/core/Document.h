#pragma once
#include "core/Layer.h"
#include "core/Selection.h"

#include <QString>
#include <QStringList>
#include <vector>

// Snapshot of everything an undoable edit can change.
struct DocState {
    std::vector<Layer> layers;
    int activeLayer = 0;
    SelectionPtr selection;
    int canvasW = 0;
    int canvasH = 0;
};

class Document {
public:
    Document(int w = 1920, int h = 1080, const QColor& bg = Qt::white, const QString& name = "Untitled");

    int width() const { return m_w; }
    int height() const { return m_h; }
    void setSize(int w, int h) { m_w = w; m_h = h; }

    QString name;
    QString filePath;
    bool dirty = false;

    std::vector<Layer>& layers() { return m_layers; }
    const std::vector<Layer>& layers() const { return m_layers; }
    int activeIndex() const { return m_active; }
    void setActive(int i);
    Layer* active();
    const Layer* active() const;

    // Layer management (indices refer to m_layers; the active index is kept in sync).
    Layer& addLayer(const QString& name, const QImage& img, int x = 0, int y = 0);
    void removeLayer(int i);
    void moveLayer(int from, int to);
    void duplicateLayer(int i);
    void mergeDown(int i);
    void flattenAll();

    SelectionPtr selection;

    DocState snapshot() const;
    void restore(const DocState& s);

    QImage composite() const;  // full canvas, premultiplied
    QImage flattenedVisible() const;

private:
    int m_w, m_h;
    int m_active = 0;
    std::vector<Layer> m_layers;
};
