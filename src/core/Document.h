#pragma once
#include "core/Color.h"
#include "core/Layer.h"
#include "core/Selection.h"

#include <string>
#include <vector>

// Everything an undoable edit can change. Layer images are shared, so snapshots are cheap.
struct DocState {
    std::vector<Layer> layers;
    int active = 0;
    SelectionPtr selection;
    int w = 0;
    int h = 0;
};

class Document {
public:
    Document(int w = 1920, int h = 1080, Color bg = rgba(255, 255, 255), std::string name = "Untitled");

    int width() const { return m_w; }
    int height() const { return m_h; }
    void setSize(int w, int h) { m_w = w; m_h = h; }

    std::string name;
    std::string filePath;

    std::vector<Layer>& layers() { return m_layers; }
    const std::vector<Layer>& layers() const { return m_layers; }
    int activeIndex() const { return m_active; }
    void setActive(int i);
    Layer* active();
    const Layer* active() const;

    int addLayer(const std::string& name, const Surface& img, int x = 0, int y = 0);
    void removeLayer(int i);
    void moveLayer(int from, int to);
    void duplicateLayer(int i);
    void mergeDown(int i);
    void flattenAll();

    SelectionPtr selection;

    DocState snapshot() const;
    void restore(const DocState& s);

    Surface flattened() const;  // visible layers composited, canvas-sized

private:
    int m_w, m_h;
    int m_active = 0;
    std::vector<Layer> m_layers;
};
