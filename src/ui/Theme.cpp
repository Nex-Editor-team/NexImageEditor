#include "ui/Theme.h"

#include <gtkmm.h>

namespace {
const char* kCss = R"CSS(
window, .background { background-color: #3c3c3c; color: #e6e6e6; }
menubar, popover menu { background-color: #2f2f2f; color: #e6e6e6; }
.toolbox button, .optionsbar, .statusbar, .panel-title { background-color: #444444; }
.toolbox button { min-width: 30px; min-height: 30px; padding: 2px; }
.toolbox button:checked { background-color: #2b5f8e; color: white; }
.optionsbar { padding: 3px 6px; border-bottom: 1px solid #262626; }
.statusbar { padding: 2px 8px; border-top: 1px solid #262626; font-size: 90%; }
.ruler-corner, .canvas-root { background-color: #2b2b2b; }
notebook > header { background-color: #2f2f2f; }
notebook > header tab { padding: 2px 8px; }
notebook > header tab:checked { background-color: #4a4a4a; }
entry, spinbutton { background-color: #2b2b2b; color: #e6e6e6; }
)CSS";
}  // namespace

void applyTheme() {
    auto provider = Gtk::CssProvider::create();
    gtk_css_provider_load_from_string(provider->gobj(), kCss);
    Gtk::StyleContext::add_provider_for_display(Gdk::Display::get_default(), provider,
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}
