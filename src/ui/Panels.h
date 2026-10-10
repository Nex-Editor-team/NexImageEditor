#pragma once
#include <QWidget>

class CanvasView;
class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QSlider;
class QLineEdit;
class QSpinBox;
class QUndoView;

// Layer list (top layer first) with add/duplicate/delete/merge/reorder buttons.
class LayersPanel : public QWidget {
    Q_OBJECT
public:
    explicit LayersPanel(QWidget* parent = nullptr);
    void setView(CanvasView* view);
    void refresh();

private:
    void onRowChanged(int row);
    void onItemChanged(class QListWidgetItem* item);
    int layerIndexForRow(int row) const;

    CanvasView* m_view = nullptr;
    QListWidget* m_list;
    bool m_updating = false;
};

// Name, opacity, blend mode, lock and position of the active layer.
class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget* parent = nullptr);
    void setView(CanvasView* view);
    void refresh();

private:
    CanvasView* m_view = nullptr;
    QLabel* m_title;
    QLineEdit* m_name;
    QSlider* m_opacity;
    QSpinBox* m_opacitySpin;
    QComboBox* m_blend;
    QCheckBox* m_locked;
    QSpinBox* m_x;
    QSpinBox* m_y;
    QLabel* m_size;
    bool m_updating = false;
};

// Undo history list.
class HistoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPanel(QWidget* parent = nullptr);
    void setView(CanvasView* view);

private:
    QUndoView* m_list;
};
