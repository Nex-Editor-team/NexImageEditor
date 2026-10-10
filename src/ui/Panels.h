#pragma once
#include "core/Document.h"
#include "core/History.h"

#include <gtkmm.h>

#include <functional>
#include <vector>

// Foreground / background colour: HSV sliders plus a hex entry (GIMP-style colour panel).
class ColorPanel {
public:
    ColorPanel(std::function<void(Color fg, Color bg)> onChange);
    Gtk::Widget& widget() { return m_root; }
    void setColors(Color fg, Color bg);

private:
    void fromSliders();
    void toSliders(Color c);
    void swap();

    std::function<void(Color, Color)> m_onChange;
    Color m_fg{0, 0, 0, 255}, m_bg{255, 255, 255, 255};
    bool m_updating = false;
    Gtk::Box m_root{Gtk::Orientation::VERTICAL, 4};
    Gtk::DrawingArea m_swatch;
    Gtk::Scale m_h, m_s, m_v;
    Gtk::Entry m_hex;
    Gtk::Button m_swap;
};

// Layers list plus properties of the active layer (blend mode, opacity).
class LayersPanel {
public:
    struct Callbacks {
        std::function<void(int)> select;
        std::function<void(int, bool)> visible;
        std::function<void(BlendMode)> blend;
        std::function<void(double)> opacity;  // 0..1, called continuously
        std::function<void(const std::string&)> action;  // new, dup, delete, up, down, merge
    };
    explicit LayersPanel(Callbacks cb);
    Gtk::Widget& widget() { return m_root; }
    void rebuild(const Document& doc);

private:
    Callbacks m_cb;
    bool m_updating = false;
    Gtk::Box m_root{Gtk::Orientation::VERTICAL, 4};
    Gtk::ListBox m_list;
    Gtk::DropDown m_blend;
    Gtk::Scale m_opacity;
    Glib::RefPtr<Gtk::Adjustment> m_opacityAdj;
};

// History list. Row 0 is the original state; row k means k applied edits.
class HistoryPanel {
public:
    explicit HistoryPanel(std::function<void(size_t)> onJump);
    Gtk::Widget& widget() { return m_scroll; }
    void rebuild(const History& h);

private:
    std::function<void(size_t)> m_onJump;
    bool m_updating = false;
    Gtk::ScrolledWindow m_scroll;
    Gtk::ListBox m_list;
};
