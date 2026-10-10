#pragma once
#include "core/History.h"
#include "core/Ops.h"
#include "tools/Tool.h"

#include <gtkmm.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct RectD {
    double x = 0, y = 0, w = 0, h = 0;
};

// One open document: canvas, rulers, pan/zoom and the active tool's interaction.
class CanvasView {
public:
    struct Hooks {
        std::function<void(CanvasView&)> changed;    // pixels, layers, selection or history changed
        std::function<void(CanvasView&)> viewMoved;  // zoom, pan or cursor changed
        std::function<void(Color)> picked;           // eyedropper result
    };

    CanvasView(Hooks hooks, Document doc);
    ~CanvasView();

    Gtk::Widget& widget() { return *m_root; }
    Document& document() { return m_doc; }
    History& history() { return m_hist; }
    std::string title() const;
    bool isClean() const { return m_hist.isClean(); }
    void markClean() { m_hist.markClean(); }
    IPoint cursor() const { return m_cursor; }

    void setTool(ToolId t, const ToolOptions& o);
    void setOptions(const ToolOptions& o) { m_opts = o; }

    void refresh();  // full update: pixels, panels and hooks
    void touch();    // visual-only update during a drag
    void zoomFit();
    void zoomBy(double factor, double anchorX, double anchorY);
    double zoom() const { return m_zoom; }

    DocState snapshot() const { return m_doc.snapshot(); }
    void recordEdit(const std::string& name, const DocState& before);
    void undo();
    void redo();
    void jumpHistory(size_t applied);

    bool hasOverlay() const { return m_cropActive || m_transformActive; }
    void commitOverlay();
    void cancelOverlay();

    void selectAll();
    void deselect();
    void invertSelection();
    void deleteSelection();
    void fillSelection(Color c);

    void setSpaceHeld(bool v) { m_spaceHeld = v; }

private:
    enum class Drag { None, Brush, Move, Marquee, Lasso, Shape, Gradient, Pan, Crop, Transform, Click };
    enum class Handle { None, Move, NW, N, NE, E, SE, S, SW, W };

    void buildWidgets();
    void draw(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h);
    void drawRuler(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h, bool horizontal);
    void drawOverlays(cairo_t* c);
    void redrawAll();
    const Surface& composite();
    Color sampleComposite(int x, int y);
    PointD toCanvas(double sx, double sy) const { return {(sx - m_panX) / m_zoom, (sy - m_panY) / m_zoom}; }
    PointD toScreen(PointD p) const { return {p.x * m_zoom + m_panX, p.y * m_zoom + m_panY}; }
    SelectionPtr combine(const SelectionPtr& add) const;
    static IRect rectFrom(PointD a, PointD b);
    Handle handleAt(double sx, double sy) const;

    void onDragBegin(double x, double y, bool shift, bool alt);
    void onDragUpdate(double dx, double dy);
    void onDragEnd(double dx, double dy);
    void clickTool(PointD canvas, PointD screen, bool shift);

    Hooks m_hooks;
    Document m_doc;
    History m_hist;
    ToolId m_tool = ToolId::Move;
    ToolOptions m_opts;

    double m_zoom = 1.0;
    double m_panX = 0, m_panY = 0;
    bool m_needsFit = true;
    bool m_spaceHeld = false;
    IPoint m_cursor{0, 0};

    Surface m_composite;
    bool m_compositeDirty = true;

    // Drag state.
    Drag m_drag = Drag::None;
    double m_startSX = 0, m_startSY = 0;  // screen start
    PointD m_last{0, 0};                  // canvas position of the last update
    double m_panStartX = 0, m_panStartY = 0;
    DocState m_before;
    int m_origLayerX = 0, m_origLayerY = 0;
    bool m_shift = false, m_alt = false;
    std::vector<IPoint> m_lasso;
    IRect m_preview{};
    bool m_hasPreview = false;
    Handle m_handle = Handle::None;
    RectD m_startBox{};

    // Overlays.
    bool m_cropActive = false;
    IRect m_crop{};
    bool m_transformActive = false;
    RectD m_tbox{};

    // Widgets.
    std::unique_ptr<Gtk::Grid> m_root;
    Gtk::DrawingArea m_area, m_rulerTop, m_rulerLeft;
    Gtk::Box m_corner;
};
