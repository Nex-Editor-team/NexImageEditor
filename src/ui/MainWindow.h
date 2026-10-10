#pragma once
#include "core/Document.h"
#include "tools/Tool.h"

#include <QMainWindow>
#include <QList>
#include <QMap>
#include <functional>
#include "core/Pixels.h"

class CanvasView;
class HistoryPanel;
class LayersPanel;
class PropertiesPanel;
class QAction;
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLabel;
class QMenu;
class QPushButton;
class QSlider;
class QSpinBox;
class QTabWidget;
class QToolBar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void openPath(const QString& path);

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    CanvasView* currentView() const;
    CanvasView* addDocument(Document doc);
    void newDocument();
    void openDocuments();
    bool saveDocument(CanvasView* view, bool asNew);
    void exportDocument();
    bool confirmClose(CanvasView* view);
    void closeTab(int index);

    void buildMenus();
    void buildToolBar();
    void buildOptionsBar();
    void updateOptionsBar();
    void showOptionsFor(ToolId id);
    void updateUi();
    void updateTabTitles();
    void updateRecentMenu();
    void pickColor(bool background);

    // Edit / select
    void copySelection();
    void pasteImage();
    void selectAll();
    void deselect();
    void invertSelection();
    void fillSelection();

    // Image / layer
    void imageSize();
    void canvasSize();
    void rotate(int degrees);
    void flip(bool horizontal);

    // Adjustments / filters
    void applyFilter(const struct FilterSpec& spec);
    void applyAdjustment(const QString& name, const std::function<void(Pixels&)>& fn);
    void adjustments(int which);
    void filters(int which);

    ToolOptions m_opts;
    QTabWidget* m_tabs;
    QToolBar* m_toolBar;
    QToolBar* m_optionsBar;
    QMap<ToolId, QAction*> m_toolActions;
    QLabel* m_zoomLabel;
    QLabel* m_sizeLabel;
    QLabel* m_posLabel;
    LayersPanel* m_layers;
    PropertiesPanel* m_props;
    HistoryPanel* m_history;
    QMenu* m_recentMenu;
    QAction* m_undoAct;
    QAction* m_redoAct;
    QAction* m_saveAct;
    QAction* m_closeAct;

    // Options bar widgets
    QSpinBox* m_brushSize;
    QSlider* m_hardness;
    QSpinBox* m_tolerance;
    QCheckBox* m_contiguous;
    QFontComboBox* m_font;
    QSpinBox* m_fontSize;
    QCheckBox* m_shapeStroke;
    QSpinBox* m_strokeWidth;
    QPushButton* m_fgBtn;
    QPushButton* m_bgBtn;
    QList<QWidget*> m_brushWidgets, m_wandWidgets, m_textWidgets, m_shapeWidgets;
    int m_untitledCount = 1;
};
