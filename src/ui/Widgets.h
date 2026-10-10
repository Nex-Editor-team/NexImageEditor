#pragma once
#include <gtkmm.h>

// Horizontal slider bound to an adjustment (uses the default constructor, which is the most stable gtkmm API).
inline Gtk::Scale* makeScale(const Glib::RefPtr<Gtk::Adjustment>& adj) {
    auto* s = Gtk::make_managed<Gtk::Scale>();
    s->set_orientation(Gtk::Orientation::HORIZONTAL);
    s->set_adjustment(adj);
    return s;
}
