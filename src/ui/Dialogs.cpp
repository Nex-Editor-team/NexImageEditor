#include "ui/Dialogs.h"

#include <cmath>
#include <memory>

namespace {

Gtk::Window* makeDialog(Gtk::Window& parent, const std::string& title) {
    auto* win = new Gtk::Window();  // destroyed by GTK when closed (set_destroy_with_parent-like via close)
    win->set_title(title);
    win->set_transient_for(parent);
    win->set_modal(true);
    win->set_resizable(false);
    return win;
}

Gtk::Box* column(Gtk::Window* win) {
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    box->set_margin(12);
    win->set_child(*box);
    return box;
}

Gtk::Box* buttonRow(Gtk::Box* parent) {
    auto* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    row->set_halign(Gtk::Align::END);
    parent->append(*row);
    return row;
}

Gtk::Box* labeled(Gtk::Box* parent, const std::string& text, Gtk::Widget& w) {
    auto* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    auto* lbl = Gtk::make_managed<Gtk::Label>(text);
    lbl->set_width_chars(12);
    lbl->set_xalign(0);
    row->append(*lbl);
    w.set_hexpand(true);
    row->append(w);
    parent->append(*row);
    return row;
}

}  // namespace

void showNewDocument(Gtk::Window& parent, std::function<void(int, int, Color, const std::string&)> onOk) {
    auto* win = makeDialog(parent, "New");
    auto* box = column(win);
    auto* name = Gtk::make_managed<Gtk::Entry>();
    name->set_text("Untitled");
    labeled(box, "Name", *name);
    auto* w = Gtk::make_managed<Gtk::SpinButton>(Gtk::Adjustment::create(1920, 1, 20000, 1, 100, 0));
    labeled(box, "Width", *w);
    auto* h = Gtk::make_managed<Gtk::SpinButton>(Gtk::Adjustment::create(1080, 1, 20000, 1, 100, 0));
    labeled(box, "Height", *h);
    auto* bg = Gtk::make_managed<Gtk::DropDown>();
    bg->set_model(Gtk::StringList::create({"White", "Black", "Transparent"}));
    labeled(box, "Background", *bg);
    auto* row = buttonRow(box);
    auto* cancel = Gtk::make_managed<Gtk::Button>("Cancel");
    auto* ok = Gtk::make_managed<Gtk::Button>("OK");
    ok->add_css_class("suggested-action");
    row->append(*cancel);
    row->append(*ok);
    cancel->signal_clicked().connect([win] { win->close(); });
    ok->signal_clicked().connect([=] {
        Color c = rgba(255, 255, 255, 255);
        if (bg->get_selected() == 1) c = rgba(0, 0, 0, 255);
        if (bg->get_selected() == 2) c = rgba(0, 0, 0, 0);
        onOk(int(w->get_value()), int(h->get_value()), c, name->get_text());
        win->close();
    });
    win->present();
}

void showSize(Gtk::Window& parent, bool imageSize, int curW, int curH,
              std::function<void(int, int, int, int)> onOk) {
    auto* win = makeDialog(parent, imageSize ? "Image Size" : "Canvas Size");
    auto* box = column(win);
    auto* w = Gtk::make_managed<Gtk::SpinButton>(Gtk::Adjustment::create(curW, 1, 20000, 1, 100, 0));
    labeled(box, "Width (px)", *w);
    auto* h = Gtk::make_managed<Gtk::SpinButton>(Gtk::Adjustment::create(curH, 1, 20000, 1, 100, 0));
    labeled(box, "Height (px)", *h);
    auto* keep = Gtk::make_managed<Gtk::CheckButton>("Keep aspect ratio");
    keep->set_active(true);
    box->append(*keep);
    const double ratio = double(curW) / curH;
    w->signal_value_changed().connect([=] {
        if (!keep->get_active()) return;
        h->set_value(std::round(w->get_value() / ratio));
    });
    h->signal_value_changed().connect([=] {
        if (!keep->get_active()) return;
        w->set_value(std::round(h->get_value() * ratio));
    });
    Gtk::DropDown* anchor = nullptr;
    if (!imageSize) {
        anchor = Gtk::make_managed<Gtk::DropDown>();
        anchor->set_model(Gtk::StringList::create({"Top Left", "Top", "Top Right", "Left", "Center", "Right",
                                                   "Bottom Left", "Bottom", "Bottom Right"}));
        anchor->set_selected(4);
        labeled(box, "Anchor", *anchor);
    }
    auto* row = buttonRow(box);
    auto* cancel = Gtk::make_managed<Gtk::Button>("Cancel");
    auto* ok = Gtk::make_managed<Gtk::Button>("OK");
    ok->add_css_class("suggested-action");
    row->append(*cancel);
    row->append(*ok);
    cancel->signal_clicked().connect([win] { win->close(); });
    ok->signal_clicked().connect([=] {
        const int nw = int(w->get_value()), nh = int(h->get_value());
        int ox = 0, oy = 0;
        if (anchor) {
            const int a = int(anchor->get_selected());
            const double fx = (a % 3) / 2.0, fy = (a / 3) / 2.0;  // 0, 0.5, 1
            ox = int(std::lround((nw - curW) * fx));
            oy = int(std::lround((nh - curH) * fy));
        }
        onOk(nw, nh, ox, oy);
        win->close();
    });
    win->present();
}

void showParams(Gtk::Window& parent, const std::string& title, const std::vector<ParamSpec>& specs,
                std::function<void(const std::vector<double>&)> onChange, std::function<void()> onOk,
                std::function<void()> onCancel) {
    auto* win = makeDialog(parent, title);
    win->set_size_request(360, -1);
    auto* box = column(win);
    auto values = std::make_shared<std::vector<double>>();
    for (const ParamSpec& s : specs) values->push_back(s.value);
    auto done = std::make_shared<bool>(false);
    for (size_t i = 0; i < specs.size(); ++i) {
        const ParamSpec& s = specs[i];
        auto adj = Gtk::Adjustment::create(s.value, s.min, s.max, s.step, s.step * 10, 0);
        auto* scale = Gtk::make_managed<Gtk::Scale>(Gtk::Orientation::HORIZONTAL, adj);
        scale->set_draw_value(true);
        scale->set_digits(s.step < 1 ? 1 : 0);
        labeled(box, s.label, *scale);
        adj->signal_value_changed().connect([=] {
            (*values)[i] = adj->get_value();
            onChange(*values);
        });
    }
    auto* row = buttonRow(box);
    auto* cancel = Gtk::make_managed<Gtk::Button>("Cancel");
    auto* ok = Gtk::make_managed<Gtk::Button>("OK");
    ok->add_css_class("suggested-action");
    row->append(*cancel);
    row->append(*ok);
    cancel->signal_clicked().connect([win] { win->close(); });
    ok->signal_clicked().connect([=] {
        *done = true;
        onOk();
        win->close();
    });
    win->signal_close_request().connect([=]() -> bool {
        if (!*done) onCancel();
        return false;
    }, false);
    onChange(*values);
    win->present();
}

void showMessage(Gtk::Window& parent, const std::string& text) {
    auto* win = makeDialog(parent, "NexImageEditor");
    auto* box = column(win);
    auto* lbl = Gtk::make_managed<Gtk::Label>(text);
    lbl->set_wrap(true);
    lbl->set_max_width_chars(60);
    box->append(*lbl);
    auto* row = buttonRow(box);
    auto* ok = Gtk::make_managed<Gtk::Button>("OK");
    row->append(*ok);
    ok->signal_clicked().connect([win] { win->close(); });
    win->present();
}

void confirmDiscard(Gtk::Window& parent, const std::string& text, std::function<void()> onDiscard) {
    auto* win = makeDialog(parent, "Unsaved changes");
    auto* box = column(win);
    auto* lbl = Gtk::make_managed<Gtk::Label>(text);
    lbl->set_wrap(true);
    box->append(*lbl);
    auto* row = buttonRow(box);
    auto* cancel = Gtk::make_managed<Gtk::Button>("Cancel");
    auto* discard = Gtk::make_managed<Gtk::Button>("Discard");
    discard->add_css_class("destructive-action");
    row->append(*cancel);
    row->append(*discard);
    cancel->signal_clicked().connect([win] { win->close(); });
    discard->signal_clicked().connect([win, onDiscard] {
        win->close();
        onDiscard();
    });
    win->present();
}
