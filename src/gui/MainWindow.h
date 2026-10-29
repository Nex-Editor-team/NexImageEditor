#pragma once
#include <QMainWindow>
#include <QUndoStack>
#include <memory>
#include "../project/Project.h"
#include "Canvas.h"
#include "LayersPanel.h"
#include "PropertiesPanel.h"
#include "PhotoAdjustmentsPanel.h"

class QLabel;
class QCloseEvent;
class QTabWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
protected:
    void closeEvent(QCloseEvent* event) override;
private slots:
    void onNewProject();
    void onOpenProject();
    void onOpenImage();
    void onSaveProject();
    void onSaveProjectAs();
    void onImportImage();
    void onExportImage();
    void onCropCanvas();
    void onAddRasterLayer();
    void onAddTextLayer();
    void onAddShapeLayer();
    void onDeleteLayer(Layer* layer);
    void onDuplicateLayer(Layer* layer);
    void onCopyLayer(Layer* layer);
    void onPasteLayer();
    void onCopyLayerToSystemClipboard();
    void onPasteImageFromSystemClipboard();
    void onMoveLayerUp(Layer* layer);
    void onMoveLayerDown(Layer* layer);
    void onLayerVisibilityToggled(Layer* layer, bool visible);
    void onLayerSelectedInPanel(Layer* layer);
    void onCanvasSelectionChanged(Layer* layer);
    void onFlipHorizontal();
    void onFlipVertical();
    void onRotate90(bool clockwise);
    void onResetTransform();
private:
    void buildMenusAndToolbar();
    void refreshTitle();
    void refreshStatus();
    void syncPanelsFromProject();
    void updateSelection(Layer* layer);
    bool confirmDiscardChanges();
    void loadStarterProject();
    std::unique_ptr<Project> m_project;
    QUndoStack* m_undoStack;
    std::unique_ptr<Layer> m_layerClipboard;
    Canvas* m_canvas;
    LayersPanel* m_layersPanel;
    PropertiesPanel* m_propertiesPanel;
    PhotoAdjustmentsPanel* m_photoPanel;
    QTabWidget* m_inspectorTabs;
    QLabel* m_documentLabel;
    QLabel* m_statusLabel;
    QLabel* m_zoomLabel;
    QString m_documentName;
};
