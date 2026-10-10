#pragma once
#include "core/Settings.h"
#include "tools/Tool.h"
#include "ui/CanvasView.h"
#include "ui/Dialogs.h"
#include "ui/Panels.h"

#include <gtkmm.h>

#include <memory>
#include <string>
#include <vector>

class MainWindow : public Gtk::ApplicationWindow {
public:
    explicit MainWindow(Gtk::Application& app);

private:
    using FilterFn = std::function<Surface(const Surface&, const std::vector<double>&)>;

    void buildMenus(Gtk::Application& app);
    void addAction(const std::string& name, std::function<void()> fn, const std::string& accel = "");
    Gtk::Box* buildToolbox();
    Gtk::Box* buildOptionBar();
    Gtk::Box* buildRightPanel();
    Gtk::Box* buildStatusBar();

    CanvasView* current() const;
    CanvasView* addDocument(Document doc);
    void newDocument();
    void openDialog();
    void openPath(const std::string& path);
    void openAsLayerPath(const std::string& path);
    void saveCurrent(bool asNew);
    void exportCurrent();
    void closeCurrent();
    void copyLayer();
    void pasteLayer();
    void editDoc(const std::string& name, const std::function<void(Document&)>& fn);
    void runFilter(const std::string& name, const std::vector<ParamSpec>& specs, FilterFn fn);
    void runAdjust(const std::string& name, std::function<Surface(const Surface&)> fn);
    void sizeDialog(bool imageSize);
    void setLayerProperty(const std::string& name, const std::function<void(Layer&)>& fn);
    void setOpacityLive(double value);

    void selectTool(ToolId t);
    void updateOptionBar();
    void syncTool();
    void schedulePanels();
    void refreshPanels();
    void updateStatus();
    void updateTitle();
    void setMessage(const std::string& msg);
    bool anyDirty() const;

    Gtk::Application& m_app;
    Glib::RefPtr<Gio::Menu> m_menuModel;
    Settings m_settings;
    std::vector<std::unique_ptr<CanvasView>> m_docs;
    Gtk::Notebook m_notebook;
    Gtk::Notebook m_panelTabs;
    Gtk::Box m_optionBar{Gtk::Orientation::HORIZONTAL, 8};
    ToolId m_tool = ToolId::Move;
    ToolOptions m_opts;
    std::vector<Gtk::ToggleButton*> m_toolButtons;
    bool m_toolGuard = false;
    bool m_forceClose = false;

    // Option bar widgets, shown per tool group.
    Gtk::Box *m_optSel = nullptr, *m_optWand = nullptr, *m_optBrush = nullptr, *m_optText = nullptr,
             *m_optShape = nullptr;
    Gtk::DropDown m_modeDrop;
    Glib::RefPtr<Gtk::Adjustment> m_tolAdj, m_brushAdj, m_fontAdj, m_strokeAdj;
    Gtk::CheckButton m_contig, m_fill, m_stroke;
    Gtk::Entry m_textEntry;

    std::unique_ptr<ColorPanel> m_color;
    std::unique_ptr<LayersPanel> m_layers;
    std::unique_ptr<HistoryPanel> m_history;

    Gtk::Label m_statusPos, m_statusSize, m_statusZoom, m_statusMsg;
    bool m_panelsPending = false;
    Surface m_clipboard;
    int m_clipX = 0, m_clipY = 0;

    // Live opacity: one history entry per slider interaction.
    bool m_opacityPending = false;
    std::shared_ptr<DocState> m_opacityBefore;
    CanvasView* m_opacityView = nullptr;
};
