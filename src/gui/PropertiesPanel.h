#pragma once
#include <QWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QFontComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QColor>
#include "../project/Layer.h"
#include "../project/Project.h"

class QUndoStack;

// Numeric counterpart to dragging the on-canvas Free Transform handles:
// lets the user type exact X/Y/scale/rotation/shear values, plus
// opacity, blend mode, and (when applicable) type-specific properties
// like text content/font/color or shape fill/stroke color. Edits here
// push proper undo commands (unlike the canvas, which only commits on
// mouse release).
class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    void setProject(Project* project) { m_project = project; }
    void setUndoStack(QUndoStack* stack) { m_undoStack = stack; }
    void setLayer(Layer* layer);    // called on selection change
    void refreshFromLayer();        // called after external edits (e.g. canvas drag)

signals:
    void requestCanvasUpdate();

private:
    void buildUi();
    void pushTransform(); // reads spin boxes, writes an undo command

    void pushTextContent();
    void pushTextFont();
    void pushTextColor();

    void pushShapeKind();
    void pushShapeSize();
    void pushShapeFillMode();
    void pushShapeFillColor();
    void pushShapeGradientColor();
    void pushShapeGradientAngle();
    void pushShapeStrokeColor();
    void pushShapeStrokeWidth();
    void pushShapeCornerRadius();
    void updateShapeRowVisibility(); // shows/hides gradient + corner-radius rows

    void pushTintColor();
    void pushTintAmount();

    void pushBgRemovalColor();
    void pushBgRemovalTolerance(int value);
    void runBackgroundRemoval();

    void pushSharpenAmount(int value);
    void pushSharpenRadius();
    void pushSharpenThreshold(int value);
    void toggleSharpenSmartMode(bool checked);
    void runSharpen();

    static void setSwatch(QPushButton* button, const QColor& color);

    Project* m_project = nullptr;
    QUndoStack* m_undoStack = nullptr;
    Layer* m_layer = nullptr;
    bool m_updatingUi = false;

    QLabel* m_nameLabel;
    QDoubleSpinBox* m_x;
    QDoubleSpinBox* m_y;
    QDoubleSpinBox* m_scaleX;
    QDoubleSpinBox* m_scaleY;
    QDoubleSpinBox* m_rotation;
    QDoubleSpinBox* m_shear;
    QSlider* m_opacity;
    QComboBox* m_blendMode;

    // ---- Universal "mix color" (tint) ----
    QGroupBox* m_tintGroup;
    QPushButton* m_tintColorBtn;
    QSlider* m_tintAmount;
    QLabel* m_tintAmountLabel;

    // ---- Text layer properties ----
    QGroupBox* m_textGroup;
    QLineEdit* m_textContent;
    QFontComboBox* m_fontFamily;
    QSpinBox* m_fontSize;
    QCheckBox* m_fontBold;
    QCheckBox* m_fontItalic;
    QPushButton* m_textColorBtn;

    // ---- Shape layer properties ----
    QGroupBox* m_shapeGroup;
    QComboBox* m_shapeKind;
    QSpinBox* m_shapeWidth;
    QSpinBox* m_shapeHeight;
    QComboBox* m_fillMode;
    QPushButton* m_fillColorBtn;
    QPushButton* m_gradientColorBtn;
    QDoubleSpinBox* m_gradientAngle;
    QPushButton* m_strokeColorBtn;
    QSpinBox* m_strokeWidth;
    QSpinBox* m_cornerRadius;

    // ---- Raster layer: background removal ----
    QGroupBox* m_bgRemovalGroup;
    QPushButton* m_bgColorBtn;
    QSlider* m_bgTolerance;
    QLabel* m_bgToleranceLabel;
    QPushButton* m_bgRemoveButton;
    QColor m_bgPickedColor{Qt::white}; // tool state, not a layer property

    // ---- Sharpen (OpenCV) ----
    QGroupBox* m_sharpenGroup;
    QSlider* m_sharpenAmount;
    QLabel* m_sharpenAmountLabel;
    QDoubleSpinBox* m_sharpenRadius;
    QSlider* m_sharpenThreshold;
    QLabel* m_sharpenThresholdLabel;
    QCheckBox* m_sharpenSmartMode;
    QSlider* m_sharpenDenoise;
    QLabel* m_sharpenDenoiseLabel;
    QPushButton* m_sharpenApplyButton;
};
