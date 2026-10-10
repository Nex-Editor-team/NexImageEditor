#include "ui/MainWindow.h"

#include "core/Adjustments.h"
#include "core/FileIO.h"
#include "core/Filters.h"
#include "core/Ops.h"
#include "ui/Dialogs.h"
#include "ui/Theme.h"

#include <gdk/gdk.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace {

std::string baseName(const std::string& path) {
    return std::filesystem::path(path).stem().string();
}

std::string toLower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

MainWindow::MainWindow(Gtk::Application& app) : m_app(app), m_settings(Settings::load()) {
    set_title("NexImageEditor");
    set_default_size(1400, 860);
    applyTheme();

    m_tolAdj = Gtk::Adjustment::create(m_opts.tolerance, 0, 255, 1, 10, 0);
    m_brushAdj = Gtk::Adjustment::create(m_opts.brushSize, 1, 300, 1, 10, 0);
    m_fontAdj = Gtk::Adjustment::create(m_opts.fontSize, 6, 400, 1, 10, 0);
    m_strokeAdj = Gtk::Adjustment::create(m_opts.strokeWidth, 1, 50, 1, 5, 0);
    m_contig.set_label("Contiguous");
    m_contig.set_active(true);
    m_fill.set_label("Fill");
    m_fill.set_active(true);
    m_stroke.set_label("Stroke");

    m_color = std::make_unique<ColorPanel>([this](Color fg, Color bg) {
        m_opts.fg = fg;
        m_opts.bg = bg;
        syncTool();
    });
    m_layers = std::make_unique<LayersPanel>(LayersPanel::Callbacks{
        [this](int i) {
            CanvasView* v = current();
            if (!v) return;
            v->document().setActive(i);
            v->cancelOverlay();
            v->touch();
            schedulePanels();
        },
        [this](int i, bool vis) {
            setLayerProperty("Layer Visibility", [i, vis](Layer& l) { l.visible = vis; });
        },
        [this](BlendMode m) {
            setLayerProperty("Blend Mode", [m](Layer& l) { l.blend = m; });
        },
        [this](double v) { setOpacityLive(v); },
        [this](const std::string& action) {
            if (action == "new") {
                editDoc("New Layer", [](Document& d) {
                    d.addLayer("Layer " + std::to_string(d.layers().size() + 1), newSurface(d.width(), d.height()));
                });
            } else if (action == "dup") {
                editDoc("Duplicate Layer", [](Document& d) { d.duplicateLayer(d.activeIndex()); });
            } else if (action == "delete") {
                editDoc("Delete Layer", [](Document& d) { d.removeLayer(d.activeIndex()); });
            } else if (action == "up") {
                editDoc("Move Layer", [](Document& d) {
                    const int a = d.activeIndex();
                    if (a + 1 < int(d.layers().size())) d.moveLayer(a, a + 1);
                });
            } else if (action == "down") {
                editDoc("Move Layer", [](Document& d) {
                    const int a = d.activeIndex();
                    if (a > 0) d.moveLayer(a, a - 1);
                });
            } else if (action == "merge") {
                editDoc("Merge Down", [](Document& d) { d.mergeDown(d.activeIndex()); });
            }
        }});
    m_history = std::make_unique<HistoryPanel>([this](size_t applied) {
        if (CanvasView* v = current()) v->jumpHistory(applied);
    });

    buildMenus(app);

    auto* root = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
    root->append(*Gtk::PopoverMenuBar::create(m_menuModel));
    root->append(*buildOptionBar());

    auto* center = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 0);
    center->append(*buildToolbox());
    m_notebook.set_scrollable(true);
    m_notebook.set_hexpand(true);
    m_notebook.set_vexpand(true);
    m_notebook.signal_switch_page().connect([this](Gtk::Widget*, guint) {
        updateStatus();
        updateTitle();
        schedulePanels();
        if (CanvasView* v = current()) v->setOptions(m_opts);
    });
    center->append(m_notebook);
    center->append(*buildRightPanel());
    root->append(*center);
    root->append(*buildStatusBar());
    set_child(*root);

    auto keys = Gtk::EventControllerKey::create();
    Gtk::EventControllerKey* keysRaw = keys.get();
    keys->signal_key_pressed().connect([this, keysRaw](guint keyval, guint, Gdk::ModifierType) -> bool {
        GdkModifierType st = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(keysRaw->gobj()));
        if (dynamic_cast<Gtk::Editable*>(get_focus())) return false;
        CanvasView* v = current();
        if (keyval == GDK_KEY_space) {
            if (v) v->setSpaceHeld(true);
            return true;
        }
        if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
            if (v) v->commitOverlay();
            return true;
        }
        if (keyval == GDK_KEY_Escape) {
            if (v) v->cancelOverlay();
            return true;
        }
        if (st & GDK_CONTROL_MASK) return false;
        const char* nm = gdk_keyval_name(keyval);
        if (nm && std::strlen(nm) == 1 && std::isalpha(static_cast<unsigned char>(nm[0]))) {
            bool found = false;
            ToolId t = toolFromShortcut(toLower(nm), found);
            if (found) {
                selectTool(t);
                return true;
            }
        }
        return false;
    }, false);
    keys->signal_key_released().connect([this](guint keyval, guint, Gdk::ModifierType) {
        if (keyval == GDK_KEY_space)
            if (CanvasView* v = current()) v->setSpaceHeld(false);
    });
    add_controller(keys);

    signal_close_request().connect([this]() -> bool {
        if (m_forceClose || !anyDirty()) return false;
        confirmDiscard(*this, "Some documents have unsaved changes. Close anyway?", [this] {
            m_forceClose = true;
            close();
        });
        return true;
    }, false);

    selectTool(ToolId::Move);
    setMessage("Ready. Open an image (Ctrl+O) or create a new document (Ctrl+N).");
    updateStatus();
}

// ---- layout ------------------------------------------------------------------

Gtk::Box* MainWindow::buildToolbox() {
    auto* col = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
    col->add_css_class("toolbox");
    col->set_margin_start(2);
    col->set_margin_end(2);
    auto* grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_column_spacing(1);
    grid->set_row_spacing(1);
    Gtk::ToggleButton* first = nullptr;
    for (int i = 0; i < int(ToolId::Count); ++i) {
        const ToolInfo& info = toolInfo(ToolId(i));
        auto* btn = Gtk::make_managed<Gtk::ToggleButton>();
        btn->set_child(*Gtk::make_managed<Gtk::Label>(info.glyph));
        btn->set_tooltip_text(std::string(info.name) + "  (" + info.shortcut + ")");
        if (!first) first = btn;
        else btn->set_group(*first);
        const ToolId id = ToolId(i);
        btn->signal_toggled().connect([this, btn, id] {
            if (m_toolGuard || !btn->get_active()) return;
            selectTool(id);
        });
        m_toolButtons.push_back(btn);
        grid->attach(*btn, i % 2, i / 2);
    }
    col->append(*grid);
    col->set_vexpand(false);
    return col;
}

Gtk::Box* MainWindow::buildOptionBar() {
    m_optionBar.add_css_class("optionsbar");

    m_optSel = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    m_optSel->append(*Gtk::make_managed<Gtk::Label>("Selection:"));
    m_modeDrop.set_model(Gtk::StringList::create({"New", "Add", "Subtract"}));
    m_modeDrop.property_selected().signal_changed().connect([this] {
        m_opts.selectionMode = SelOp(m_modeDrop.get_selected());
        syncTool();
    });
    m_optSel->append(m_modeDrop);

    m_optWand = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    m_optWand->append(*Gtk::make_managed<Gtk::Label>("Tolerance:"));
    auto* tol = Gtk::make_managed<Gtk::Scale>(Gtk::Orientation::HORIZONTAL, m_tolAdj);
    tol->set_size_request(160, -1);
    tol->set_draw_value(true);
    tol->set_digits(0);
    m_tolAdj->signal_value_changed().connect([this] {
        m_opts.tolerance = int(m_tolAdj->get_value());
        syncTool();
    });
    m_optWand->append(*tol);
    m_contig.signal_toggled().connect([this] {
        m_opts.contiguous = m_contig.get_active();
        syncTool();
    });
    m_optWand->append(m_contig);

    m_optBrush = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    m_optBrush->append(*Gtk::make_managed<Gtk::Label>("Size:"));
    auto* brush = Gtk::make_managed<Gtk::Scale>(Gtk::Orientation::HORIZONTAL, m_brushAdj);
    brush->set_size_request(160, -1);
    brush->set_draw_value(true);
    brush->set_digits(0);
    m_brushAdj->signal_value_changed().connect([this] {
        m_opts.brushSize = m_brushAdj->get_value();
        syncTool();
    });
    m_optBrush->append(*brush);

    m_optText = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    m_textEntry.set_text(m_opts.text);
    m_textEntry.set_width_chars(20);
    m_textEntry.signal_changed().connect([this] {
        m_opts.text = m_textEntry.get_text();
        syncTool();
    });
    m_optText->append(*Gtk::make_managed<Gtk::Label>("Text:"));
    m_optText->append(m_textEntry);
    auto* fontSize = Gtk::make_managed<Gtk::SpinButton>(m_fontAdj);
    fontSize->signal_value_changed().connect([this, fontSize] {
        m_opts.fontSize = int(fontSize->get_value());
        syncTool();
    });
    m_optText->append(*Gtk::make_managed<Gtk::Label>("Size:"));
    m_optText->append(*fontSize);

    m_optShape = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    m_fill.signal_toggled().connect([this] {
        m_opts.shapeFill = m_fill.get_active();
        syncTool();
    });
    m_stroke.signal_toggled().connect([this] {
        m_opts.shapeStroke = m_stroke.get_active();
        syncTool();
    });
    auto* sw = Gtk::make_managed<Gtk::SpinButton>(m_strokeAdj);
    sw->signal_value_changed().connect([this, sw] {
        m_opts.strokeWidth = sw->get_value();
        syncTool();
    });
    m_optShape->append(m_fill);
    m_optShape->append(m_stroke);
    m_optShape->append(*Gtk::make_managed<Gtk::Label>("Width:"));
    m_optShape->append(*sw);

    m_optionBar.append(*Gtk::make_managed<Gtk::Label>("Options:"));
    for (Gtk::Box* b : {m_optSel, m_optWand, m_optBrush, m_optText, m_optShape}) m_optionBar.append(*b);
    return &m_optionBar;
}

Gtk::Box* MainWindow::buildRightPanel() {
    auto* col = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
    col->set_size_request(300, -1);
    col->append(m_color->widget());
    m_panelTabs.set_hexpand(true);
    m_panelTabs.set_vexpand(true);
    m_panelTabs.append_page(m_layers->widget(), "Layers");
    m_panelTabs.append_page(m_history->widget(), "History");
    col->append(m_panelTabs);
    return col;
}

Gtk::Box* MainWindow::buildStatusBar() {
    auto* bar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 16);
    bar->add_css_class("statusbar");
    m_statusMsg.set_hexpand(true);
    m_statusMsg.set_xalign(0);
    bar->append(m_statusPos);
    bar->append(m_statusSize);
    bar->append(m_statusZoom);
    bar->append(m_statusMsg);
    return bar;
}

void MainWindow::addAction(const std::string& name, std::function<void()> fn, const std::string& accel) {
    auto action = Gio::SimpleAction::create(name);
    action->signal_activate().connect([fn](const Glib::VariantBase&) { fn(); });
    add_action(action);
    if (!accel.empty()) m_app.set_accels_for_action("win." + name, {accel});
}

void MainWindow::buildMenus(Gtk::Application& app) {
    (void)app;
    auto menu = Gio::Menu::create();
    auto add = [](const Glib::RefPtr<Gio::Menu>& m, const char* label, const char* action) {
        m->append(label, action);
    };

    auto file = Gio::Menu::create();
    add(file, "New…", "win.new");
    add(file, "Open…", "win.open");
    add(file, "Open as Layer…", "win.open-layer");
    add(file, "Save", "win.save");
    add(file, "Save As…", "win.save-as");
    add(file, "Export…", "win.export");
    add(file, "Close", "win.close");
    menu->append_submenu("File", file);

    auto edit = Gio::Menu::create();
    add(edit, "Undo", "win.undo");
    add(edit, "Redo", "win.redo");
    add(edit, "Copy Layer", "win.copy");
    add(edit, "Paste as Layer", "win.paste");
    add(edit, "Fill with Foreground", "win.fill");
    add(edit, "Delete", "win.delete");
    add(edit, "Free Transform", "win.transform");
    menu->append_submenu("Edit", edit);

    auto image = Gio::Menu::create();
    add(image, "Canvas Size…", "win.canvas-size");
    add(image, "Image Size…", "win.image-size");
    add(image, "Crop to Selection", "win.crop-selection");
    add(image, "Rotate 90° Clockwise", "win.rot-cw");
    add(image, "Rotate 90° Counter-clockwise", "win.rot-ccw");
    add(image, "Rotate 180°", "win.rot-180");
    add(image, "Flip Horizontal", "win.flip-h");
    add(image, "Flip Vertical", "win.flip-v");
    add(image, "Flatten Image", "win.flatten");
    menu->append_submenu("Image", image);

    auto layer = Gio::Menu::create();
    add(layer, "New Layer", "win.layer-new");
    add(layer, "Duplicate Layer", "win.layer-dup");
    add(layer, "Delete Layer", "win.layer-del");
    add(layer, "Merge Down", "win.layer-merge");
    add(layer, "Move Up", "win.layer-up");
    add(layer, "Move Down", "win.layer-down");
    menu->append_submenu("Layer", layer);

    auto select = Gio::Menu::create();
    add(select, "All", "win.select-all");
    add(select, "Deselect", "win.deselect");
    add(select, "Invert", "win.invert");
    menu->append_submenu("Select", select);

    auto filters = Gio::Menu::create();
    add(filters, "Gaussian Blur…", "win.blur");
    add(filters, "Unsharp Mask…", "win.unsharp");
    add(filters, "Median…", "win.median");
    add(filters, "Add Noise…", "win.noise");
    menu->append_submenu("Filters", filters);

    auto colors = Gio::Menu::create();
    add(colors, "Invert", "win.adj-invert");
    add(colors, "Grayscale", "win.adj-gray");
    add(colors, "Brightness / Contrast…", "win.adj-bc");
    add(colors, "Hue / Saturation…", "win.adj-hs");
    add(colors, "Threshold…", "win.adj-threshold");
    add(colors, "Posterize…", "win.adj-posterize");
    menu->append_submenu("Colors", colors);

    auto view = Gio::Menu::create();
    add(view, "Zoom In", "win.zoom-in");
    add(view, "Zoom Out", "win.zoom-out");
    add(view, "Fit on Screen", "win.zoom-fit");
    add(view, "Actual Size (100%)", "win.zoom-100");
    menu->append_submenu("View", view);

    m_menuModel = menu;

    addAction("new", [this] { newDocument(); }, "<Control>n");
    addAction("open", [this] { openDialog(); }, "<Control>o");
    addAction("open-layer", [this] {
        if (!current()) return;
        auto dlg = Gtk::FileDialog::create();
        dlg->open(*this, [this, dlg](const Glib::RefPtr<Gio::AsyncResult>& r) {
            try {
                auto f = dlg->open_finish(r);
                if (f) openAsLayerPath(f->get_path());
            } catch (const Glib::Error&) {
            }
        });
    });
    addAction("save", [this] { saveCurrent(false); }, "<Control>s");
    addAction("save-as", [this] { saveCurrent(true); }, "<Control><Shift>s");
    addAction("export", [this] { exportCurrent(); }, "<Control><Shift>e");
    addAction("close", [this] { closeCurrent(); }, "<Control>w");

    addAction("undo", [this] { if (auto* v = current()) v->undo(); }, "<Control>z");
    addAction("redo", [this] { if (auto* v = current()) v->redo(); }, "<Control><Shift>z");
    addAction("copy", [this] { copyLayer(); }, "<Control>c");
    addAction("paste", [this] { pasteLayer(); }, "<Control>v");
    addAction("fill", [this] { if (auto* v = current()) v->fillSelection(m_opts.fg); }, "<Alt>BackSpace");
    addAction("delete", [this] { if (auto* v = current()) v->deleteSelection(); }, "Delete");
    addAction("transform", [this] { selectTool(ToolId::Transform); }, "<Control>t");

    addAction("canvas-size", [this] { sizeDialog(false); });
    addAction("image-size", [this] { sizeDialog(true); });
    addAction("crop-selection", [this] {
        CanvasView* v = current();
        if (!v || !v->document().selection) return;
        IRect b = v->document().selection->bounds();
        if (b.empty()) return;
        DocState s = v->snapshot();
        ops::cropDocument(v->document(), b);
        v->recordEdit("Crop to Selection", s);
    });
    addAction("rot-cw", [this] { editDoc("Rotate Canvas", [](Document& d) { ops::rotateCanvas90(d, true); }); });
    addAction("rot-ccw", [this] { editDoc("Rotate Canvas", [](Document& d) { ops::rotateCanvas90(d, false); }); });
    addAction("rot-180", [this] { editDoc("Rotate Canvas", [](Document& d) { ops::rotateCanvas180(d); }); });
    addAction("flip-h", [this] { editDoc("Flip Canvas", [](Document& d) { ops::flipCanvas(d, true); }); });
    addAction("flip-v", [this] { editDoc("Flip Canvas", [](Document& d) { ops::flipCanvas(d, false); }); });
    addAction("flatten", [this] { editDoc("Flatten Image", [](Document& d) { d.flattenAll(); }); });

    addAction("layer-new", [this] {
        editDoc("New Layer", [](Document& d) {
            d.addLayer("Layer " + std::to_string(d.layers().size() + 1), newSurface(d.width(), d.height()));
        });
    }, "<Control><Shift>n");
    addAction("layer-dup", [this] { editDoc("Duplicate Layer", [](Document& d) { d.duplicateLayer(d.activeIndex()); }); },
              "<Control>j");
    addAction("layer-del", [this] { editDoc("Delete Layer", [](Document& d) { d.removeLayer(d.activeIndex()); }); });
    addAction("layer-merge", [this] { editDoc("Merge Down", [](Document& d) { d.mergeDown(d.activeIndex()); }); },
              "<Control>e");
    addAction("layer-up", [this] {
        editDoc("Move Layer", [](Document& d) {
            const int a = d.activeIndex();
            if (a + 1 < int(d.layers().size())) d.moveLayer(a, a + 1);
        });
    });
    addAction("layer-down", [this] {
        editDoc("Move Layer", [](Document& d) {
            const int a = d.activeIndex();
            if (a > 0) d.moveLayer(a, a - 1);
        });
    });

    addAction("select-all", [this] { if (auto* v = current()) v->selectAll(); }, "<Control>a");
    addAction("deselect", [this] { if (auto* v = current()) v->deselect(); }, "<Control>d");
    addAction("invert", [this] { if (auto* v = current()) v->invertSelection(); }, "<Control>i");

    addAction("blur", [this] {
        runFilter("Gaussian Blur", {{"Radius", 0.5, 50, 2, 0.5}},
                  [](const Surface& s, const std::vector<double>& p) { return filters::gaussianBlur(s, p[0]); });
    });
    addAction("unsharp", [this] {
        runFilter("Unsharp Mask", {{"Radius", 0.5, 20, 2, 0.5}, {"Amount %", 0, 500, 100, 1}, {"Threshold", 0, 255, 0, 1}},
                  [](const Surface& s, const std::vector<double>& p) {
                      return filters::unsharpMask(s, p[0], p[1], int(p[2]));
                  });
    });
    addAction("median", [this] {
        runFilter("Median", {{"Radius", 1, 10, 2, 1}},
                  [](const Surface& s, const std::vector<double>& p) { return filters::median(s, int(p[0])); });
    });
    addAction("noise", [this] {
        runFilter("Add Noise", {{"Amount %", 0, 100, 10, 1}, {"Monochrome", 0, 1, 1, 1}},
                  [](const Surface& s, const std::vector<double>& p) {
                      return filters::addNoise(s, p[0], p[1] >= 0.5, 1234u);
                  });
    });
    addAction("adj-invert", [this] { runAdjust("Invert", [](const Surface& s) { return adjust::invert(s); }); });
    addAction("adj-gray", [this] { runAdjust("Grayscale", [](const Surface& s) { return adjust::grayscale(s); }); });
    addAction("adj-bc", [this] {
        runFilter("Brightness / Contrast", {{"Brightness", -100, 100, 0, 1}, {"Contrast", -100, 100, 0, 1}},
                  [](const Surface& s, const std::vector<double>& p) {
                      return adjust::brightnessContrast(s, int(p[0]), int(p[1]));
                  });
    });
    addAction("adj-hs", [this] {
        runFilter("Hue / Saturation",
                  {{"Hue", -180, 180, 0, 1}, {"Saturation", -100, 100, 0, 1}, {"Lightness", -100, 100, 0, 1}},
                  [](const Surface& s, const std::vector<double>& p) {
                      return adjust::hueSaturation(s, int(p[0]), int(p[1]), int(p[2]));
                  });
    });
    addAction("adj-threshold", [this] {
        runFilter("Threshold", {{"Level", 1, 255, 128, 1}},
                  [](const Surface& s, const std::vector<double>& p) { return adjust::threshold(s, int(p[0])); });
    });
    addAction("adj-posterize", [this] {
        runFilter("Posterize", {{"Levels", 2, 255, 4, 1}},
                  [](const Surface& s, const std::vector<double>& p) { return adjust::posterize(s, int(p[0])); });
    });

    addAction("zoom-in", [this] { if (auto* v = current()) v->zoomBy(1.25, v->widget().get_width() / 2.0, v->widget().get_height() / 2.0); },
              "<Control>plus");
    addAction("zoom-out", [this] { if (auto* v = current()) v->zoomBy(0.8, v->widget().get_width() / 2.0, v->widget().get_height() / 2.0); },
              "<Control>minus");
    addAction("zoom-fit", [this] { if (auto* v = current()) v->zoomFit(); }, "<Control>0");
    addAction("zoom-100", [this] {
        if (auto* v = current()) v->zoomBy(1.0 / v->zoom(), v->widget().get_width() / 2.0, v->widget().get_height() / 2.0);
    }, "<Control>1");

    addAction("quit", [this] { close(); }, "<Control>q");
}

// ---- documents ---------------------------------------------------------------

CanvasView* MainWindow::current() const {
    const int idx = m_notebook.get_current_page();
    if (idx < 0 || idx >= int(m_docs.size())) return nullptr;
    return m_docs[size_t(idx)].get();
}

CanvasView* MainWindow::addDocument(Document doc) {
    CanvasView::Hooks hooks;
    hooks.changed = [this](CanvasView& v) {
        m_notebook.set_tab_label_text(v.widget(), v.title());
        if (&v == current()) {
            schedulePanels();
            updateStatus();
            updateTitle();
        }
    };
    hooks.viewMoved = [this](CanvasView& v) {
        if (&v == current()) updateStatus();
    };
    hooks.picked = [this](Color c) {
        m_opts.fg = c;
        m_color->setColors(m_opts.fg, m_opts.bg);
        syncTool();
        setMessage("Foreground colour picked.");
    };
    auto view = std::make_unique<CanvasView>(hooks, std::move(doc));
    CanvasView* raw = view.get();
    m_docs.push_back(std::move(view));
    m_notebook.append_page(raw->widget(), raw->title());
    m_notebook.set_tab_label_text(raw->widget(), raw->title());
    m_notebook.set_current_page(int(m_docs.size()) - 1);
    raw->setOptions(m_opts);
    raw->setTool(m_tool, m_opts);
    raw->zoomFit();
    schedulePanels();
    updateStatus();
    updateTitle();
    return raw;
}

void MainWindow::newDocument() {
    showNewDocument(*this, [this](int w, int h, Color bg, const std::string& name) {
        addDocument(Document(w, h, bg, name.empty() ? "Untitled" : name));
        setMessage("New document created.");
    });
}

void MainWindow::openDialog() {
    auto dlg = Gtk::FileDialog::create();
    dlg->set_title("Open image or project");
    dlg->open(*this, [this, dlg](const Glib::RefPtr<Gio::AsyncResult>& r) {
        try {
            auto f = dlg->open_finish(r);
            if (f) openPath(f->get_path());
        } catch (const Glib::Error&) {
            // cancelled
        }
    });
}

void MainWindow::openPath(const std::string& path) {
    if (fileio::isProjectPath(path)) {
        Document d;
        std::string err;
        if (!fileio::loadProject(path, d, err)) {
            showMessage(*this, "Could not open project: " + err);
            return;
        }
        d.name = baseName(path);
        addDocument(std::move(d));
    } else {
        std::string err;
        Surface s = fileio::importImage(path, err);
        if (!s) {
            showMessage(*this, "Could not open image: " + err);
            return;
        }
        Document d(surfW(s), surfH(s), rgba(0, 0, 0, 0), baseName(path));
        d.layers()[0].img = s;
        d.layers()[0].name = "Background";
        addDocument(std::move(d));
    }
    m_settings.addRecent(path);
    m_settings.save();
    setMessage("Opened " + path);
}

void MainWindow::openAsLayerPath(const std::string& path) {
    std::string err;
    Surface s = fileio::importImage(path, err);
    if (!s) {
        showMessage(*this, "Could not open image: " + err);
        return;
    }
    editDoc("Open as Layer", [&](Document& d) { d.addLayer(baseName(path), s, 0, 0); });
}

void MainWindow::saveCurrent(bool asNew) {
    CanvasView* v = current();
    if (!v) return;
    auto save = [this, v](const std::string& path) {
        std::string err;
        if (!fileio::saveProject(path, v->document(), err)) {
            showMessage(*this, "Save failed: " + err);
            return;
        }
        v->document().filePath = path;
        v->document().name = baseName(path);
        v->markClean();
        m_settings.addRecent(path);
        m_settings.save();
        v->refresh();
        setMessage("Saved " + path);
    };
    if (!asNew && !v->document().filePath.empty()) {
        save(v->document().filePath);
        return;
    }
    auto dlg = Gtk::FileDialog::create();
    dlg->set_title("Save project");
    dlg->set_initial_name(v->document().name + ".nxe");
    dlg->save(*this, [this, dlg, save](const Glib::RefPtr<Gio::AsyncResult>& r) {
        try {
            auto f = dlg->save_finish(r);
            if (!f) return;
            std::string path = f->get_path();
            if (!fileio::isProjectPath(path)) path += ".nxe";
            save(path);
        } catch (const Glib::Error&) {
        }
    });
}

void MainWindow::exportCurrent() {
    CanvasView* v = current();
    if (!v) return;
    auto dlg = Gtk::FileDialog::create();
    dlg->set_title("Export image");
    dlg->set_initial_name(v->document().name + ".png");
    dlg->save(*this, [this, dlg, v](const Glib::RefPtr<Gio::AsyncResult>& r) {
        try {
            auto f = dlg->save_finish(r);
            if (!f) return;
            std::string err;
            if (!fileio::exportImage(f->get_path(), v->document().flattened(), err))
                showMessage(*this, "Export failed: " + err);
            else
                setMessage("Exported " + f->get_path());
        } catch (const Glib::Error&) {
        }
    });
}

void MainWindow::closeCurrent() {
    const int idx = m_notebook.get_current_page();
    if (idx < 0) return;
    auto doClose = [this, idx] {
        m_notebook.remove_page(idx);
        m_docs.erase(m_docs.begin() + idx);
        updateStatus();
        updateTitle();
        schedulePanels();
    };
    CanvasView* v = current();
    if (v && !v->isClean()) confirmDiscard(*this, "\"" + v->document().name + "\" has unsaved changes.", doClose);
    else doClose();
}

void MainWindow::copyLayer() {
    CanvasView* v = current();
    if (!v || !v->document().active()) return;
    const Layer* L = v->document().active();
    Layer t;
    t.img = cloneSurface(L->img);
    t.x = L->x;
    t.y = L->y;
    if (const SelectionPtr sel = v->document().selection) {
        // Keep only the selected pixels: clear everything outside the selection.
        const int W = v->document().width(), H = v->document().height();
        std::vector<uint8_t> inv(size_t(W) * size_t(H));
        for (size_t i = 0; i < inv.size(); ++i) inv[i] = uint8_t(255 - sel->mask[i]);
        if (SelectionPtr outside = Selection::fromMask(std::move(inv), W, H))
            ops::applyResult(t, newSurface(L->w(), L->h()), outside.get());
    }
    m_clipboard = t.img;
    m_clipX = t.x;
    m_clipY = t.y;
    setMessage("Layer copied.");
}

void MainWindow::pasteLayer() {
    if (!m_clipboard) return;
    editDoc("Paste", [this](Document& d) { d.addLayer("Pasted layer", m_clipboard, m_clipX, m_clipY); });
}

void MainWindow::editDoc(const std::string& name, const std::function<void(Document&)>& fn) {
    CanvasView* v = current();
    if (!v) return;
    DocState before = v->snapshot();
    fn(v->document());
    v->recordEdit(name, before);
}

void MainWindow::setLayerProperty(const std::string& name, const std::function<void(Layer&)>& fn) {
    editDoc(name, [&fn](Document& d) {
        if (Layer* l = d.active()) fn(*l);
    });
}

void MainWindow::setOpacityLive(double value) {
    CanvasView* v = current();
    Layer* L = v ? v->document().active() : nullptr;
    if (!L) return;
    if (!m_opacityPending) {
        m_opacityPending = true;
        m_opacityBefore = std::make_shared<DocState>(v->snapshot());
        m_opacityView = v;
    }
    L->opacity = value;
    v->touch();
    // Record one history entry once the slider has been idle for a moment.
    Glib::signal_timeout().connect_once([this] {
        if (!m_opacityPending) return;
        m_opacityPending = false;
        if (m_opacityView) {
            bool alive = false;
            for (auto& d : m_docs) alive = alive || d.get() == m_opacityView;
            if (alive) m_opacityView->recordEdit("Opacity", *m_opacityBefore);
        }
    }, 500);
}

void MainWindow::runAdjust(const std::string& name, std::function<Surface(const Surface&)> fn) {
    CanvasView* v = current();
    if (!v) return;
    Layer* L = v->document().active();
    if (!L || L->locked) return;
    DocState before = v->snapshot();
    ops::applyResult(*L, fn(L->img), v->document().selection.get());
    v->recordEdit(name, before);
}

void MainWindow::runFilter(const std::string& name, const std::vector<ParamSpec>& specs, FilterFn fn) {
    CanvasView* v = current();
    if (!v) return;
    Layer* L = v->document().active();
    if (!L || L->locked) return;
    const int idx = v->document().activeIndex();
    Surface base = L->img;
    SelectionPtr sel = v->document().selection;
    auto before = std::make_shared<DocState>(v->snapshot());
    auto apply = [v, idx, base, sel, fn](const std::vector<double>& p) {
        Layer tmp;
        tmp.img = base;
        ops::applyResult(tmp, fn(base, p), sel.get());
        v->document().layers()[size_t(idx)].img = tmp.img;
        v->touch();
    };
    showParams(
        *this, name, specs, apply,
        [v, before, name] { v->recordEdit(name, *before); },
        [v, idx, base] {
            v->document().layers()[size_t(idx)].img = base;
            v->touch();
        });
}

void MainWindow::sizeDialog(bool imageSize) {
    CanvasView* v = current();
    if (!v) return;
    const int W = v->document().width(), H = v->document().height();
    showSize(*this, imageSize, W, H, [this, imageSize](int w, int h, int ox, int oy) {
        if (imageSize) editDoc("Image Size", [w, h](Document& d) { ops::resizeImage(d, w, h); });
        else editDoc("Canvas Size", [w, h, ox, oy](Document& d) { ops::resizeCanvas(d, w, h, ox, oy); });
    });
}

// ---- tools and panels --------------------------------------------------------

void MainWindow::selectTool(ToolId t) {
    m_tool = t;
    m_toolGuard = true;
    if (size_t(t) < m_toolButtons.size()) m_toolButtons[size_t(t)]->set_active(true);
    m_toolGuard = false;
    updateOptionBar();
    syncTool();
    setMessage(std::string(toolInfo(t).name) + " tool");
}

void MainWindow::updateOptionBar() {
    const bool sel = isSelectionTool(m_tool);
    const bool wand = m_tool == ToolId::MagicWand || m_tool == ToolId::Bucket;
    const bool brush = m_tool == ToolId::Brush || m_tool == ToolId::Eraser;
    const bool text = m_tool == ToolId::Text;
    const bool shape = m_tool == ToolId::Rectangle || m_tool == ToolId::Ellipse;
    m_optSel->set_visible(sel);
    m_optWand->set_visible(wand);
    m_optBrush->set_visible(brush);
    m_optText->set_visible(text);
    m_optShape->set_visible(shape);
}

void MainWindow::syncTool() {
    for (auto& d : m_docs) d->setTool(m_tool, m_opts);
    if (CanvasView* v = current()) v->setOptions(m_opts);
}

void MainWindow::schedulePanels() {
    if (m_panelsPending) return;
    m_panelsPending = true;
    // Deferred: rebuilding a list from inside one of its own row callbacks is unsafe.
    Glib::signal_idle().connect_once([this] {
        m_panelsPending = false;
        refreshPanels();
    });
}

void MainWindow::refreshPanels() {
    CanvasView* v = current();
    if (!v) return;
    m_layers->rebuild(v->document());
    m_history->rebuild(v->history());
}

void MainWindow::updateStatus() {
    CanvasView* v = current();
    if (!v) {
        m_statusPos.set_text("");
        m_statusSize.set_text("");
        m_statusZoom.set_text("");
        return;
    }
    const IPoint c = v->cursor();
    m_statusPos.set_text("X: " + std::to_string(c.x) + "  Y: " + std::to_string(c.y));
    m_statusSize.set_text("Doc: " + std::to_string(v->document().width()) + " × " +
                          std::to_string(v->document().height()) + " px");
    char buf[32];
    std::snprintf(buf, sizeof buf, "Zoom: %.0f%%", v->zoom() * 100);
    m_statusZoom.set_text(buf);
}

void MainWindow::updateTitle() {
    CanvasView* v = current();
    set_title(v ? "NexImageEditor — " + v->title() : "NexImageEditor");
}

void MainWindow::setMessage(const std::string& msg) { m_statusMsg.set_text(msg); }

bool MainWindow::anyDirty() const {
    for (const auto& d : m_docs)
        if (!d->isClean()) return true;
    return false;
}
