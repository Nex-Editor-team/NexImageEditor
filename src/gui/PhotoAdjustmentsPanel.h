#pragma once
#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QScrollArea>
#include "../project/Layer.h"
#include "../project/Project.h"
#include "../engine/ToneParameters.h"
#include "HistogramWidget.h"
#include "CurveWidget.h"

class QUndoStack;

class PhotoAdjustmentsPanel : public QWidget {
    Q_OBJECT
public:
    explicit PhotoAdjustmentsPanel(QWidget* parent = nullptr);

    void setProject(Project* project) { m_project = project; }
    void setUndoStack(QUndoStack* stack) { m_undoStack = stack; }
    void setLayer(Layer* layer);
    void refreshFromLayer();
    void updateHistogram();

signals:
    void requestCanvasUpdate();

private:
    void buildUi();
    void setupDarkStyle();

    // Slider row creation helper
    struct SliderRow {
        QWidget* container;
        QLabel* titleLabel;
        QLabel* valueLabel;
        QSlider* slider;
        double scale = 1.0;
        int decimals = 0;
    };

    SliderRow createSliderRow(QWidget* parent, const QString& title, int min, int max, int defaultVal,
                              double scale = 1.0, int decimals = 0, const QString& unit = "");

    void connectSliderRow(SliderRow& row, std::function<void(double)> onLiveChange, std::function<void()> onCommit);

    QWidget* createCollapsibleSection(const QString& title, QWidget* contentWidget, bool expanded = true);

    void updateValueLabel(SliderRow& row, int rawValue);

    void onAutoToneClicked();
    void onBwModeToggled(bool checked);
    void onHdrModeToggled(bool checked);
    void onProfileSelected(int index);
    void onProfileAmountChanged(int value);
    void onResetAllClicked();
    void onBakeClicked();
    void onCopyAdjustments();   // copies current layer's ToneParameters to app-wide clipboard
    void onPasteAdjustments();  // pastes clipboard ToneParameters to current layer

    static bool s_clipboardHasData;          // true when something was copied
    static ToneParameters s_clipboard;       // app-wide clipboard (static across all instances)

    void applyLiveAdjustments();
    void pushUndoCommand(const QString& text = "Photo Adjustments");

    Project* m_project = nullptr;
    QUndoStack* m_undoStack = nullptr;
    Layer* m_layer = nullptr;
    bool m_updatingUi = false;
    ToneParameters m_savedBeforeParams; // for undo capture

    // Header & Quick Modes
    HistogramWidget* m_histogram;
    QPushButton* m_btnAuto;
    QPushButton* m_btnBw;
    QPushButton* m_btnHdr;
    QPushButton* m_btnInfo;

    // Profiles / Presets
    QComboBox* m_profileCombo;
    QPushButton* m_btnProfileGrid;
    QSlider* m_profileAmountSlider;
    QLabel* m_profileAmountLabel;

    // Light section
    SliderRow m_rowExposure;
    SliderRow m_rowContrast;
    SliderRow m_rowHighlights;
    SliderRow m_rowShadows;
    SliderRow m_rowWhites;
    SliderRow m_rowBlacks;

    // Color section
    SliderRow m_rowTemp;
    SliderRow m_rowTint;
    SliderRow m_rowVibrance;
    SliderRow m_rowSaturation;

    // Effects section
    SliderRow m_rowClarity;
    SliderRow m_rowDehaze;
    SliderRow m_rowVignette;

    // Curves section
    CurveWidget* m_curveWidget;

    // Footer actions
    QPushButton* m_btnResetAll;
    QPushButton* m_btnBake;
    QPushButton* m_btnCopy;
    QPushButton* m_btnPaste;
    QCheckBox* m_chkLivePreview;
};
