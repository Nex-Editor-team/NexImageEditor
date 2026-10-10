#pragma once
#include <QMainWindow>
#include <QUndoStack>
#include <QDockWidget>
#include <memory>
#include "../project/Project.h"
#include "Canvas.h"
#include "LayersPanel.h"
#include "PropertiesPanel.h"
#include "PhotoAdjustmentsPanel.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();
    void onImportImage();
    void onExportImage();
    void onResizeCanvas();
    void onAbout();
    void onFilter(const QString& name);
    void onCrop(QRect rect);

    void onAddRasterLayer();
    void onAddTextLayer();
    void onAddShapeLayer();
    void onDeleteLayer(Layer* layer);
    void onDuplicateLayer(Layer* layer);
    void onCopyLayer(Layer* layer);                 // copy "trong" app -> clipboard nội bộ
    void onPasteLayer();                             // dán từ clipboard nội bộ (layer mới)
    void onCopyLayerToSystemClipboard();              // copy "ra ngoài" -> clipboard hệ thống (ảnh)
    void onPasteImageFromSystemClipboard();           // dán ảnh từ clipboard hệ thống (layer mới)
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
    void showLayerContextMenu(Layer* layer, QPoint globalPosition);
    void importImages(const QStringList& paths, QPointF position);
    void buildMenusAndToolbar();
    void refreshTitle();
    bool maybeSave();
    void refreshAfterCommand();
    void syncPanelsFromProject();
    void updateSelection(Layer* layer); // shows/hides Properties & Adjustments dock + syncs panels

    std::unique_ptr<Project> m_project;
    QUndoStack* m_undoStack;
    std::unique_ptr<Layer> m_layerClipboard; // clipboard nội bộ cho Copy/Paste layer trong app

    Canvas* m_canvas;
    LayersPanel* m_layersPanel;
    PropertiesPanel* m_propertiesPanel;
    QDockWidget* m_propertiesDock;
    PhotoAdjustmentsPanel* m_photoPanel;
    QDockWidget* m_photoDock;
};
