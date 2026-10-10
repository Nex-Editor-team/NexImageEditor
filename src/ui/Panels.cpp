#include "ui/Panels.h"

#include "ui/Widgets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

void hsvToRgb(double h, double s, double v, Color& c) {
    h = std::fmod(h, 360.0);
    if (h < 0) h += 360;
    const double C = v * s, X = C * (1 - std::abs(std::fmod(h / 60.0, 2.0) - 1)), m = v - C;
    double r = 0, g = 0, b = 0;
    if (h < 60) { r = C; g = X; }
    else if (h < 120) { r = X; g = C; }
    else if (h < 180) { g = C; b = X; }
    else if (h < 240) { g = X; b = C; }
    else if (h < 300) { r = X; b = C; }
    else { r = C; b = X; }
    c.r = uint8_t(std::lround((r + m) * 255));
    c.g = uint8_t(std::lround((g + m) * 255));
    c.b = uint8_t(std::lround((b + m) * 255));
}

void rgbToHsv(Color c, double& h, double& s, double& v) {
    const double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
    const double mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
    v = mx;
    s = mx == 0 ? 0 : d / mx;
    if (d == 0) h = 0;
    else if (mx == r) h = 60 * std::fmod((g - b) / d, 6.0);
    else if (mx == g) h = 60 * ((b - r) / d + 2);
    else h = 60 * ((r - g) / d + 4);
    if (h < 0) h += 360;
}

std::string hexOf(Color c) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "#%02X%02X%02X", c.r, c.g, c.b);
    return buf;
}

bool parseHex(const std::string& s, Color& out) {
    std::string t = s;
    if (!t.empty() && t[0] == '#') t = t.substr(1);
    if (t.size() != 6) return false;
    unsigned r, g, b;
    if (std::sscanf(t.c_str(), "%2x%2x%2x", &r, &g, &b) != 3) return false;
    out = Color{uint8_t(r), uint8_t(g), uint8_t(b), 255};
    return true;
}

const std::vector<Glib::ustring> kBlendNames = {"Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten",
                                                "Color Dodge", "Color Burn", "Hard Light", "Soft Light",
                                                "Difference", "Exclusion"};

}  // namespace

// ---- ColorPanel ------------------------------------------------------------

ColorPanel::ColorPanel(std::function<void(Color, Color)> onChange) : m_onChange(std::move(onChange)) {
    m_swatch.set_content_width(120);
    m_swatch.set_content_height(60);
    m_swatch.set_draw_func([this](const Cairo::RefPtr<Cairo::Context>& cr, int w, int h) {
        cairo_t* c = cr->cobj();
        const double s = std::min(w, h) * 0.6;
        // Background swatch behind the foreground one (GIMP/Photoshop layout).
        cairo_set_source_rgb(c, m_bg.r / 255.0, m_bg.g / 255.0, m_bg.b / 255.0);
        cairo_rectangle(c, w - s - 6, h - s - 6, s, s);
        cairo_fill(c);
        cairo_set_source_rgb(c, m_fg.r / 255.0, m_fg.g / 255.0, m_fg.b / 255.0);
        cairo_rectangle(c, 6, 6, s, s);
        cairo_fill(c);
        cairo_set_source_rgb(c, 0.1, 0.1, 0.1);
        cairo_set_line_width(c, 1);
        cairo_rectangle(c, 6.5, 6.5, s - 1, s - 1);
        cairo_stroke(c);
    });
    m_root.set_margin(6);
    m_root.append(m_swatch);
    m_root.append(m_hex);
    for (auto* p : {&m_h, &m_s, &m_v}) {
        p->set_draw_value(true);
        p->set_digits(0);
        p->set_hexpand(true);
        m_root.append(*p);
    }
    m_h.set_range(0, 360);
    m_s.set_range(0, 100);
    m_v.set_range(0, 100);
    m_swap.set_label("\u21C4 Swap");
    m_swap.set_tooltip_text("Swap foreground and background (X)");
    m_root.append(m_swap);

    m_h.signal_value_changed().connect([this] { fromSliders(); });
    m_s.signal_value_changed().connect([this] { fromSliders(); });
    m_v.signal_value_changed().connect([this] { fromSliders(); });
    m_hex.signal_activate().connect([this] {
        Color c;
        if (parseHex(m_hex.get_text(), c)) {
            m_fg = c;
            m_fg.a = 255;
            toSliders(m_fg);
            if (m_onChange) m_onChange(m_fg, m_bg);
        }
    });
    m_swap.signal_clicked().connect([this] { swap(); });
    toSliders(m_fg);
}

void ColorPanel::setColors(Color fg, Color bg) {
    m_bg = bg;
    toSliders(fg);
}

void ColorPanel::toSliders(Color c) {
    m_fg = c;
    m_updating = true;
    double h, s, v;
    rgbToHsv(c, h, s, v);
    m_h.set_value(h);
    m_s.set_value(s * 100);
    m_v.set_value(v * 100);
    m_hex.set_text(hexOf(c));
    m_updating = false;
    m_swatch.queue_draw();
}

void ColorPanel::fromSliders() {
    if (m_updating) return;
    hsvToRgb(m_h.get_value(), m_s.get_value() / 100, m_v.get_value() / 100, m_fg);
    m_fg.a = 255;
    m_hex.set_text(hexOf(m_fg));
    m_swatch.queue_draw();
    if (m_onChange) m_onChange(m_fg, m_bg);
}

void ColorPanel::swap() {
    std::swap(m_fg, m_bg);
    toSliders(m_fg);
    if (m_onChange) m_onChange(m_fg, m_bg);
}

// ---- LayersPanel -----------------------------------------------------------

LayersPanel::LayersPanel(Callbacks cb) : m_cb(std::move(cb)) {
    m_root.set_margin(6);
    auto* buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 2);
    struct B { const char* label; const char* action; };
    for (const B& b : {B{"+", "new"}, B{"\u2750", "dup"}, B{"\u2716", "delete"}, B{"\u25B2", "up"},
                       B{"\u25BC", "down"}, B{"\u2A5D", "merge"}}) {
        auto* btn = Gtk::make_managed<Gtk::Button>(b.label);
        const std::string action = b.action;
        btn->signal_clicked().connect([this, action] {
            if (m_cb.action) m_cb.action(action);
        });
        buttons->append(*btn);
    }
    m_root.append(*buttons);

    m_list.set_selection_mode(Gtk::SelectionMode::SINGLE);
    m_list.signal_row_selected().connect([this](Gtk::ListBoxRow* row) {
        if (m_updating || !row) return;
        if (m_cb.select) m_cb.select(row->get_index());
    });
    auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_child(m_list);
    scroll->set_vexpand(true);
    scroll->set_min_content_height(180);
    m_root.append(*scroll);

    m_blend.set_model(Gtk::StringList::create(kBlendNames));
    m_blend.property_selected().signal_changed().connect([this] {
        if (!m_updating && m_cb.blend) m_cb.blend(BlendMode(m_blend.get_selected()));
    });
    m_root.append(m_blend);

    m_opacityAdj = Gtk::Adjustment::create(100, 0, 100, 1, 10, 0);
    m_opacity.set_adjustment(m_opacityAdj);
    m_opacity.set_draw_value(true);
    m_opacity.set_digits(0);
    m_opacityAdj->signal_value_changed().connect([this] {
        if (!m_updating && m_cb.opacity) m_cb.opacity(m_opacityAdj->get_value() / 100.0);
    });
    m_root.append(m_opacity);
}

void LayersPanel::rebuild(const Document& doc) {
    m_updating = true;
    while (auto* row = m_list.get_row_at_index(0)) m_list.remove(*row);
    const auto& layers = doc.layers();
    // Top layer first, as in Photoshop/GIMP.
    for (int i = int(layers.size()) - 1; i >= 0; --i) {
        const Layer& l = layers[size_t(i)];
        auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
        auto* eye = Gtk::make_managed<Gtk::CheckButton>();
        eye->set_active(l.visible);
        eye->set_tooltip_text("Show / hide layer");
        eye->signal_toggled().connect([this, i, eye] {
            if (!m_updating && m_cb.visible) m_cb.visible(i, eye->get_active());
        });
        auto* name = Gtk::make_managed<Gtk::Label>(l.name);
        name->set_xalign(0);
        name->set_hexpand(true);
        box->append(*eye);
        box->append(*name);
        if (l.background) {
            auto* lock = Gtk::make_managed<Gtk::Label>("\U0001F512");
            box->append(*lock);
        }
        auto* row = Gtk::make_managed<Gtk::ListBoxRow>();
        row->set_child(*box);
        m_list.append(*row);
        if (i == doc.activeIndex()) m_list.select_row(*row);
    }
    if (const Layer* a = doc.active()) {
        m_blend.set_selected(guint(a->blend));
        m_opacityAdj->set_value(a->opacity * 100);
    }
    m_updating = false;
}

// ---- HistoryPanel ----------------------------------------------------------

HistoryPanel::HistoryPanel(std::function<void(size_t)> onJump) : m_onJump(std::move(onJump)) {
    m_list.set_selection_mode(Gtk::SelectionMode::SINGLE);
    m_list.signal_row_selected().connect([this](Gtk::ListBoxRow* row) {
        if (m_updating || !row) return;
        if (m_onJump) m_onJump(size_t(row->get_index()));
    });
    m_scroll.set_child(m_list);
    m_scroll.set_vexpand(true);
    m_scroll.set_min_content_height(160);
}

void HistoryPanel::rebuild(const History& h) {
    m_updating = true;
    while (auto* row = m_list.get_row_at_index(0)) m_list.remove(*row);
    auto add = [this](const std::string& text) {
        auto* lbl = Gtk::make_managed<Gtk::Label>(text);
        lbl->set_xalign(0);
        lbl->set_margin(4);
        auto* row = Gtk::make_managed<Gtk::ListBoxRow>();
        row->set_child(*lbl);
        m_list.append(*row);
        return row;
    };
    add("Original");
    for (const auto& e : h.entries()) add(e.name);
    if (auto* row = m_list.get_row_at_index(int(h.appliedCount()))) m_list.select_row(*row);
    m_updating = false;
}
