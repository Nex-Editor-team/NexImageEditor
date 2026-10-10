#pragma once
#include <QWidget>
#include <QListWidget>
#include "../project/Project.h"

class LayersPanel : public QWidget {
    Q_OBJECT
public:
    explicit LayersPanel(QWidget* parent = nullptr);

    void setProject(Project* project);
    void refresh();               // rebuild list from project
    void selectLayer(Layer* layer);

signals:
    void layerContextMenuRequested(Layer* layer, QPoint globalPosition);
    void layerSelected(Layer* layer);
    void addRasterRequested();
    void addTextRequested();
    void addShapeRequested();
    void deleteRequested(Layer* layer);
    void duplicateRequested(Layer* layer);
    void copyRequested(Layer* layer);   // copy layer đang chọn (clipboard nội bộ app)
    void pasteRequested();              // dán layer từ clipboard nội bộ app
    void moveUpRequested(Layer* layer);
    void moveDownRequested(Layer* layer);
    void visibilityToggled(Layer* layer, bool visible);

private:
    Project* m_project = nullptr;
    QListWidget* m_list;

    Layer* layerForItem(QListWidgetItem* item) const;
};
