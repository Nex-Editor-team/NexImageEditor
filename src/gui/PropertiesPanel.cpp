#include "PropertiesPanel.h"
#include "../project/Commands.h"
#include "../project/TextLayer.h"
#include "../project/ShapeLayer.h"
#include "../engine/BackgroundRemoval.h"
#include "../engine/CvSharpen.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QUndoStack>
#include <QColorDialog>
#include <QScrollArea>
#include <QFrame>

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent) {
    buildUi();
    setLayer(nullptr);
}

void PropertiesPanel::setSwatch(QPushButton* button, const QColor& color) {
    QString textColor = color.lightness() > 128 ? "black" : "white";
    button->setStyleSheet(QString("background-color: %1; color: %2;")
                               .arg(color.name(QColor::HexArgb), textColor));
    button->setText(color.name(QColor::HexArgb));
}

void PropertiesPanel::buildUi() {
    // The panel's content routinely grows taller than the dock's visible
    // area (Transform + Opacity/Blend + Color Mix + Text/Shape +
    // Background Removal groups all at once), so everything lives inside
    // a scroll area rather than being clipped at the bottom.
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* content = new QWidget(scrollArea);
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(4, 4, 4, 4);

    m_nameLabel = new QLabel("<b>No layer selected</b>");
    root->addWidget(m_nameLabel);

    auto* form = new QFormLayout();
    root->addLayout(form);

    auto makeSpin = [this](double lo, double hi, double step) {
        auto* sb = new QDoubleSpinBox(this);
        sb->setRange(lo, hi);
        sb->setSingleStep(step);
        sb->setDecimals(2);
        connect(sb, &QDoubleSpinBox::editingFinished, this, &PropertiesPanel::pushTransform);
        return sb;
    };

    m_x = makeSpin(-100000, 100000, 1.0);
    m_y = makeSpin(-100000, 100000, 1.0);
    m_scaleX = makeSpin(-100.0, 100.0, 0.05);
    m_scaleY = makeSpin(-100.0, 100.0, 0.05);
    m_rotation = makeSpin(-3600, 3600, 1.0);
    m_shear = makeSpin(-89.0, 89.0, 1.0);

    form->addRow("Position X", m_x);
    form->addRow("Position Y", m_y);
    form->addRow("Scale X", m_scaleX);
    form->addRow("Scale Y", m_scaleY);
    form->addRow("Rotation °", m_rotation);
    form->addRow("Shear °", m_shear);

    m_opacity = new QSlider(Qt::Horizontal, this);
    m_opacity->setRange(0, 100);
    form->addRow("Opacity", m_opacity);
    connect(m_opacity, &QSlider::valueChanged, this, [this](int v) {
        if (m_updatingUi || !m_layer || !m_project) return;
        double before = m_layer->opacity;
        double after = v / 100.0;
        if (before == after) return;
        m_layer->opacity = after; // live preview
        emit requestCanvasUpdate();
        if (m_undoStack) {
            auto* cmd = new SetLayerPropertyCommand<double>(
                m_project, m_layer->id(), before, after,
                [](Layer* l) { return l->opacity; },
                [](Layer* l, double val) { l->opacity = val; },
                "Change Opacity");
            m_undoStack->push(cmd);
        }
    });

    m_blendMode = new QComboBox(this);
    for (BlendMode bm : {BlendMode::Normal, BlendMode::Multiply, BlendMode::Screen, BlendMode::Overlay,
                          BlendMode::Darken, BlendMode::Lighten, BlendMode::Difference,
                          BlendMode::ColorDodge, BlendMode::ColorBurn})
        m_blendMode->addItem(Blend::toString(bm));
    form->addRow("Blend Mode", m_blendMode);
    connect(m_blendMode, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (m_updatingUi || !m_layer || !m_project) return;
        BlendMode before = m_layer->blendMode;
        BlendMode after = Blend::fromString(text);
        if (before == after) return;
        m_layer->blendMode = after;
        emit requestCanvasUpdate();
        if (m_undoStack) {
            auto* cmd = new SetLayerPropertyCommand<BlendMode>(
                m_project, m_layer->id(), before, after,
                [](Layer* l) { return l->blendMode; },
                [](Layer* l, BlendMode val) { l->blendMode = val; },
                "Change Blend Mode");
            m_undoStack->push(cmd);
        }
    });

    // ---- Universal "mix color" (tint) ----
    m_tintGroup = new QGroupBox("Color Mix", this);
    auto* tintForm = new QFormLayout(m_tintGroup);

    m_tintColorBtn = new QPushButton(m_tintGroup);
    connect(m_tintColorBtn, &QPushButton::clicked, this, &PropertiesPanel::pushTintColor);
    tintForm->addRow("Mix Color", m_tintColorBtn);

    auto* tintAmountRow = new QHBoxLayout();
    m_tintAmount = new QSlider(Qt::Horizontal, m_tintGroup);
    m_tintAmount->setRange(0, 100);
    m_tintAmountLabel = new QLabel("0%", m_tintGroup);
    tintAmountRow->addWidget(m_tintAmount);
    tintAmountRow->addWidget(m_tintAmountLabel);
    connect(m_tintAmount, &QSlider::valueChanged, this, [this](int v) {
        m_tintAmountLabel->setText(QString("%1%").arg(v));
        pushTintAmount();
    });
    tintForm->addRow("Amount", tintAmountRow);

    root->addWidget(m_tintGroup);

    // ---- Text group ----
    m_textGroup = new QGroupBox("Text", this);
    auto* textForm = new QFormLayout(m_textGroup);

    m_textContent = new QLineEdit(m_textGroup);
    connect(m_textContent, &QLineEdit::editingFinished, this, &PropertiesPanel::pushTextContent);
    textForm->addRow("Content", m_textContent);

    m_fontFamily = new QFontComboBox(m_textGroup);
    connect(m_fontFamily, &QFontComboBox::currentFontChanged, this, [this](const QFont&) { pushTextFont(); });
    textForm->addRow("Font", m_fontFamily);

    m_fontSize = new QSpinBox(m_textGroup);
    m_fontSize->setRange(1, 2000);
    connect(m_fontSize, &QSpinBox::editingFinished, this, &PropertiesPanel::pushTextFont);
    textForm->addRow("Size", m_fontSize);

    auto* styleRow = new QHBoxLayout();
    m_fontBold = new QCheckBox("Bold", m_textGroup);
    m_fontItalic = new QCheckBox("Italic", m_textGroup);
    connect(m_fontBold, &QCheckBox::toggled, this, [this](bool) { pushTextFont(); });
    connect(m_fontItalic, &QCheckBox::toggled, this, [this](bool) { pushTextFont(); });
    styleRow->addWidget(m_fontBold);
    styleRow->addWidget(m_fontItalic);
    textForm->addRow("Style", styleRow);

    m_textColorBtn = new QPushButton(m_textGroup);
    connect(m_textColorBtn, &QPushButton::clicked, this, &PropertiesPanel::pushTextColor);
    textForm->addRow("Color", m_textColorBtn);

    root->addWidget(m_textGroup);
    m_textGroup->setVisible(false);

    // ---- Shape group ----
    m_shapeGroup = new QGroupBox("Shape", this);
    auto* shapeForm = new QFormLayout(m_shapeGroup);

    m_shapeKind = new QComboBox(m_shapeGroup);
    m_shapeKind->addItem("Rectangle");
    m_shapeKind->addItem("Rounded Rectangle");
    m_shapeKind->addItem("Ellipse");
    m_shapeKind->addItem("Triangle");
    m_shapeKind->addItem("Pentagon");
    m_shapeKind->addItem("Hexagon");
    m_shapeKind->addItem("Star");
    m_shapeKind->addItem("Arrow");
    m_shapeKind->addItem("Line");
    connect(m_shapeKind, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { pushShapeKind(); });
    shapeForm->addRow("Kind", m_shapeKind);

    m_shapeWidth = new QSpinBox(m_shapeGroup);
    m_shapeWidth->setRange(1, 20000);
    connect(m_shapeWidth, &QSpinBox::editingFinished, this, &PropertiesPanel::pushShapeSize);
    shapeForm->addRow("Width", m_shapeWidth);

    m_shapeHeight = new QSpinBox(m_shapeGroup);
    m_shapeHeight->setRange(1, 20000);
    connect(m_shapeHeight, &QSpinBox::editingFinished, this, &PropertiesPanel::pushShapeSize);
    shapeForm->addRow("Height", m_shapeHeight);

    m_cornerRadius = new QSpinBox(m_shapeGroup);
    m_cornerRadius->setRange(0, 5000);
    connect(m_cornerRadius, &QSpinBox::editingFinished, this, &PropertiesPanel::pushShapeCornerRadius);
    shapeForm->addRow("Corner Radius", m_cornerRadius);

    m_fillMode = new QComboBox(m_shapeGroup);
    m_fillMode->addItem("Solid");
    m_fillMode->addItem("Linear Gradient");
    m_fillMode->addItem("Radial Gradient");
    connect(m_fillMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { pushShapeFillMode(); });
    shapeForm->addRow("Fill Mode", m_fillMode);

    m_fillColorBtn = new QPushButton(m_shapeGroup);
    connect(m_fillColorBtn, &QPushButton::clicked, this, &PropertiesPanel::pushShapeFillColor);
    shapeForm->addRow("Fill Color", m_fillColorBtn);

    m_gradientColorBtn = new QPushButton(m_shapeGroup);
    connect(m_gradientColorBtn, &QPushButton::clicked, this, &PropertiesPanel::pushShapeGradientColor);
    shapeForm->addRow("Mix With", m_gradientColorBtn);

    m_gradientAngle = new QDoubleSpinBox(m_shapeGroup);
    m_gradientAngle->setRange(-360.0, 360.0);
    m_gradientAngle->setDecimals(1);
    connect(m_gradientAngle, &QDoubleSpinBox::editingFinished, this, &PropertiesPanel::pushShapeGradientAngle);
    shapeForm->addRow("Gradient Angle °", m_gradientAngle);

    m_strokeColorBtn = new QPushButton(m_shapeGroup);
    connect(m_strokeColorBtn, &QPushButton::clicked, this, &PropertiesPanel::pushShapeStrokeColor);
    shapeForm->addRow("Stroke Color", m_strokeColorBtn);

    m_strokeWidth = new QSpinBox(m_shapeGroup);
    m_strokeWidth->setRange(0, 500);
    connect(m_strokeWidth, &QSpinBox::editingFinished, this, &PropertiesPanel::pushShapeStrokeWidth);
    shapeForm->addRow("Stroke Width", m_strokeWidth);

    root->addWidget(m_shapeGroup);
    m_shapeGroup->setVisible(false);

    // ---- Background Removal group (raster layers only) ----
    m_bgRemovalGroup = new QGroupBox("Background Removal", this);
    auto* bgForm = new QFormLayout(m_bgRemovalGroup);

    m_bgColorBtn = new QPushButton(m_bgRemovalGroup);
    setSwatch(m_bgColorBtn, m_bgPickedColor);
    connect(m_bgColorBtn, &QPushButton::clicked, this, &PropertiesPanel::pushBgRemovalColor);
    bgForm->addRow("Background Color", m_bgColorBtn);

    auto* toleranceRow = new QHBoxLayout();
    m_bgTolerance = new QSlider(Qt::Horizontal, m_bgRemovalGroup);
    m_bgTolerance->setRange(0, 100);
    m_bgTolerance->setValue(20);
    m_bgToleranceLabel = new QLabel("20%", m_bgRemovalGroup);
    toleranceRow->addWidget(m_bgTolerance);
    toleranceRow->addWidget(m_bgToleranceLabel);
    connect(m_bgTolerance, &QSlider::valueChanged, this, &PropertiesPanel::pushBgRemovalTolerance);
    bgForm->addRow("Tolerance", toleranceRow);

    m_bgRemoveButton = new QPushButton("Remove Background", m_bgRemovalGroup);
    connect(m_bgRemoveButton, &QPushButton::clicked, this, &PropertiesPanel::runBackgroundRemoval);
    bgForm->addRow(m_bgRemoveButton);

    root->addWidget(m_bgRemovalGroup);
    m_bgRemovalGroup->setVisible(false);

    // ---- Sharpen (OpenCV) group ----
    m_sharpenGroup = new QGroupBox("Sharpen (OpenCV)", this);
    auto* sharpenForm = new QFormLayout(m_sharpenGroup);

    auto* amountRow = new QHBoxLayout();
    m_sharpenAmount = new QSlider(Qt::Horizontal, m_sharpenGroup);
    m_sharpenAmount->setRange(0, 300); // 0.00 .. 3.00
    m_sharpenAmount->setValue(100);
    m_sharpenAmountLabel = new QLabel("1.00", m_sharpenGroup);
    amountRow->addWidget(m_sharpenAmount);
    amountRow->addWidget(m_sharpenAmountLabel);
    connect(m_sharpenAmount, &QSlider::valueChanged, this, &PropertiesPanel::pushSharpenAmount);
    sharpenForm->addRow("Amount", amountRow);

    m_sharpenRadius = new QDoubleSpinBox(m_sharpenGroup);
    m_sharpenRadius->setRange(0.1, 20.0);
    m_sharpenRadius->setSingleStep(0.1);
    m_sharpenRadius->setValue(1.5);
    connect(m_sharpenRadius, &QDoubleSpinBox::editingFinished, this, &PropertiesPanel::pushSharpenRadius);
    sharpenForm->addRow("Radius", m_sharpenRadius);

    auto* thresholdRow = new QHBoxLayout();
    m_sharpenThreshold = new QSlider(Qt::Horizontal, m_sharpenGroup);
    m_sharpenThreshold->setRange(0, 255);
    m_sharpenThreshold->setValue(0);
    m_sharpenThresholdLabel = new QLabel("0", m_sharpenGroup);
    thresholdRow->addWidget(m_sharpenThreshold);
    thresholdRow->addWidget(m_sharpenThresholdLabel);
    connect(m_sharpenThreshold, &QSlider::valueChanged, this, &PropertiesPanel::pushSharpenThreshold);
    sharpenForm->addRow("Threshold", thresholdRow);

    m_sharpenSmartMode = new QCheckBox("Smart (edge-preserving denoise first)", m_sharpenGroup);
    connect(m_sharpenSmartMode, &QCheckBox::toggled, this, &PropertiesPanel::toggleSharpenSmartMode);
    sharpenForm->addRow(m_sharpenSmartMode);

    auto* denoiseRow = new QHBoxLayout();
    m_sharpenDenoise = new QSlider(Qt::Horizontal, m_sharpenGroup);
    m_sharpenDenoise->setRange(0, 100);
    m_sharpenDenoise->setValue(40);
    m_sharpenDenoiseLabel = new QLabel("40%", m_sharpenGroup);
    denoiseRow->addWidget(m_sharpenDenoise);
    denoiseRow->addWidget(m_sharpenDenoiseLabel);
    connect(m_sharpenDenoise, &QSlider::valueChanged, this, [this](int v) {
        m_sharpenDenoiseLabel->setText(QString("%1%").arg(v));
    });
    sharpenForm->addRow("Denoise Strength", denoiseRow);
    m_sharpenDenoise->setEnabled(false);

    m_sharpenApplyButton = new QPushButton("Apply Sharpen", m_sharpenGroup);
    connect(m_sharpenApplyButton, &QPushButton::clicked, this, &PropertiesPanel::runSharpen);
    sharpenForm->addRow(m_sharpenApplyButton);

    root->addWidget(m_sharpenGroup);

    root->addStretch(1);

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
}

void PropertiesPanel::updateShapeRowVisibility() {
    bool isShape = m_layer && m_layer->type() == LayerType::Shape;
    bool isLine = isShape && static_cast<ShapeLayer*>(m_layer)->kind == ShapeKind::Line;
    bool isRoundedRect = isShape && static_cast<ShapeLayer*>(m_layer)->kind == ShapeKind::RoundedRectangle;
    bool isGradient = isShape && static_cast<ShapeLayer*>(m_layer)->fillMode != ShapeFillMode::Solid;

    auto setRowVisible = [this](QWidget* field, bool visible) {
        if (auto* form = qobject_cast<QFormLayout*>(m_shapeGroup->layout())) {
            if (QWidget* lbl = form->labelForField(field)) lbl->setVisible(visible);
        }
        field->setVisible(visible);
    };

    // Fill controls don't apply to a Line (it only has a stroke).
    setRowVisible(m_fillMode, !isLine);
    setRowVisible(m_fillColorBtn, !isLine);
    setRowVisible(m_gradientColorBtn, !isLine && isGradient);
    setRowVisible(m_gradientAngle, !isLine && isGradient);
    setRowVisible(m_cornerRadius, isRoundedRect);
}

void PropertiesPanel::setLayer(Layer* layer) {
    m_layer = layer;
    bool enabled = layer != nullptr;
    QWidget* widgets[] = {m_x, m_y, m_scaleX, m_scaleY, m_rotation, m_shear, m_opacity, m_blendMode,
                           m_tintColorBtn, m_tintAmount};
    for (QWidget* w : widgets)
        w->setEnabled(enabled);
    m_sharpenGroup->setEnabled(enabled);

    m_nameLabel->setText(layer ? QString("<b>%1</b>").arg(layer->name) : "<b>No layer selected</b>");

    m_textGroup->setVisible(layer && layer->type() == LayerType::Text);
    m_shapeGroup->setVisible(layer && layer->type() == LayerType::Shape);
    m_bgRemovalGroup->setVisible(layer && layer->type() == LayerType::Raster);

    refreshFromLayer();
}

void PropertiesPanel::refreshFromLayer() {
    m_updatingUi = true;
    if (m_layer) {
        m_x->setValue(m_layer->transform.center.x());
        m_y->setValue(m_layer->transform.center.y());
        m_scaleX->setValue(m_layer->transform.scaleX);
        m_scaleY->setValue(m_layer->transform.scaleY);
        m_rotation->setValue(m_layer->transform.rotationDeg);
        m_shear->setValue(m_layer->transform.shearXDeg);
        m_opacity->setValue(int(m_layer->opacity * 100));
        m_blendMode->setCurrentText(Blend::toString(m_layer->blendMode));

        setSwatch(m_tintColorBtn, m_layer->tintColor);
        int tintPct = int(m_layer->tintAmount * 100);
        m_tintAmount->setValue(tintPct);
        m_tintAmountLabel->setText(QString("%1%").arg(tintPct));

        if (m_layer->type() == LayerType::Text) {
            auto* tl = static_cast<TextLayer*>(m_layer);
            m_textContent->setText(tl->text);
            m_fontFamily->setCurrentFont(tl->font);
            m_fontSize->setValue(tl->font.pointSize() > 0 ? tl->font.pointSize() : 32);
            m_fontBold->setChecked(tl->font.bold());
            m_fontItalic->setChecked(tl->font.italic());
            setSwatch(m_textColorBtn, tl->color);
        } else if (m_layer->type() == LayerType::Shape) {
            auto* sl = static_cast<ShapeLayer*>(m_layer);
            m_shapeKind->setCurrentIndex(int(sl->kind));
            m_shapeWidth->setValue(sl->localSize.width());
            m_shapeHeight->setValue(sl->localSize.height());
            m_cornerRadius->setValue(sl->cornerRadius);
            m_fillMode->setCurrentIndex(int(sl->fillMode));
            setSwatch(m_fillColorBtn, sl->fillColor);
            setSwatch(m_gradientColorBtn, sl->fillColor2);
            m_gradientAngle->setValue(sl->gradientAngleDeg);
            setSwatch(m_strokeColorBtn, sl->strokeColor);
            m_strokeWidth->setValue(sl->strokeWidth);
        }
    } else {
        m_x->setValue(0); m_y->setValue(0);
        m_scaleX->setValue(1); m_scaleY->setValue(1);
        m_rotation->setValue(0); m_shear->setValue(0);
        m_opacity->setValue(100);
        m_blendMode->setCurrentIndex(0);
        m_tintAmount->setValue(0);
        m_tintAmountLabel->setText("0%");
    }
    updateShapeRowVisibility();
    m_updatingUi = false;
}

void PropertiesPanel::pushTransform() {
    if (m_updatingUi || !m_layer || !m_project) return;

    Transform::FreeTransformState before = m_layer->transform;
    Transform::FreeTransformState after = before;
    after.center = {m_x->value(), m_y->value()};
    after.scaleX = m_scaleX->value();
    after.scaleY = m_scaleY->value();
    after.rotationDeg = m_rotation->value();
    after.shearXDeg = m_shear->value();

    if (after == before) return;

    m_layer->transform = after; // live preview
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new TransformLayerCommand(m_project, m_layer->id(), before, after);
        m_undoStack->push(cmd);
    }
}

// ---------------- Text properties ----------------

void PropertiesPanel::pushTextContent() {
    if (m_updatingUi || !m_layer || !m_project || m_layer->type() != LayerType::Text) return;
    auto* tl = static_cast<TextLayer*>(m_layer);

    QString before = tl->text;
    QString after = m_textContent->text();
    if (before == after) return;

    tl->text = after;
    tl->regenerate();
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<QString>(
            m_project, m_layer->id(), before, after,
            [](Layer* l) { return static_cast<TextLayer*>(l)->text; },
            [](Layer* l, QString val) {
                auto* t = static_cast<TextLayer*>(l);
                t->text = val;
                t->regenerate();
            },
            "Edit Text");
        m_undoStack->push(cmd);
    }
}

void PropertiesPanel::pushTextFont() {
    if (m_updatingUi || !m_layer || !m_project || m_layer->type() != LayerType::Text) return;
    auto* tl = static_cast<TextLayer*>(m_layer);

    QFont before = tl->font;
    QFont after = m_fontFamily->currentFont();
    after.setPointSize(m_fontSize->value());
    after.setBold(m_fontBold->isChecked());
    after.setItalic(m_fontItalic->isChecked());
    if (before == after) return;

    tl->font = after;
    tl->regenerate();
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<QFont>(
            m_project, m_layer->id(), before, after,
            [](Layer* l) { return static_cast<TextLayer*>(l)->font; },
            [](Layer* l, QFont val) {
                auto* t = static_cast<TextLayer*>(l);
                t->font = val;
                t->regenerate();
            },
            "Change Font");
        m_undoStack->push(cmd);
    }
}

void PropertiesPanel::pushTextColor() {
    if (!m_layer || !m_project || m_layer->type() != LayerType::Text) return;
    auto* tl = static_cast<TextLayer*>(m_layer);

    QColor before = tl->color;
    QColor chosen = QColorDialog::getColor(before, this, "Text Color", QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid() || chosen == before) return;

    tl->color = chosen;
    tl->regenerate();
    setSwatch(m_textColorBtn, chosen);
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<QColor>(
            m_project, m_layer->id(), before, chosen,
            [](Layer* l) { return static_cast<TextLayer*>(l)->color; },
            [](Layer* l, QColor val) {
                auto* t = static_cast<TextLayer*>(l);
                t->color = val;
                t->regenerate();
            },
            "Change Text Color");
        m_undoStack->push(cmd);
    }
}

// ---------------- Shape properties ----------------

void PropertiesPanel::pushShapeKind() {
    if (m_updatingUi || !m_layer || !m_project || m_layer->type() != LayerType::Shape) return;
    auto* sl = static_cast<ShapeLayer*>(m_layer);

    ShapeKind before = sl->kind;
    ShapeKind after = ShapeKind(m_shapeKind->currentIndex());
    if (before == after) return;

    sl->kind = after;
    sl->regenerate();
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<ShapeKind>(
            m_project, m_layer->id(), before, after,
            [](Layer* l) { return static_cast<ShapeLayer*>(l)->kind; },
            [](Layer* l, ShapeKind val) {
                auto* s = static_cast<ShapeLayer*>(l);
                s->kind = val;
                s->regenerate();
            },
            "Change Shape Kind");
        m_undoStack->push(cmd);
    }
    updateShapeRowVisibility();
}

void PropertiesPanel::pushShapeSize() {
    if (m_updatingUi || !m_layer || !m_project || m_layer->type() != LayerType::Shape) return;
    auto* sl = static_cast<ShapeLayer*>(m_layer);

    QSize before = sl->localSize;
    QSize after(m_shapeWidth->value(), m_shapeHeight->value());
    if (before == after) return;

    sl->localSize = after;
    sl->regenerate();
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<QSize>(
            m_project, m_layer->id(), before, after,
            [](Layer* l) { return static_cast<ShapeLayer*>(l)->localSize; },
            [](Layer* l, QSize val) {
                auto* s = static_cast<ShapeLayer*>(l);
                s->localSize = val;
                s->regenerate();
            },
            "Resize Shape");
        m_undoStack->push(cmd);
    }
}

void PropertiesPanel::pushShapeFillMode() {
    if (m_updatingUi || !m_layer || !m_project || m_layer->type() != LayerType::Shape) return;
    auto* sl = static_cast<ShapeLayer*>(m_layer);

    ShapeFillMode before = sl->fillMode;
    ShapeFillMode after = ShapeFillMode(m_fillMode->currentIndex());
    if (before == after) return;

    sl->fillMode = after;
    sl->regenerate();
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<ShapeFillMode>(
            m_project, m_layer->id(), before, after,
            [](Layer* l) { return static_cast<ShapeLayer*>(l)->fillMode; },
            [](Layer* l, ShapeFillMode val) {
                auto* s = static_cast<ShapeLayer*>(l);
                s->fillMode = val;
                s->regenerate();
            },
            "Change Fill Mode");
        m_undoStack->push(cmd);
    }
    updateShapeRowVisibility();
}

void PropertiesPanel::pushShapeGradientColor() {
    if (!m_layer || !m_project || m_layer->type() != LayerType::Shape) return;
    auto* sl = static_cast<ShapeLayer*>(m_layer);

    QColor before = sl->fillColor2;
    QColor chosen = QColorDialog::getColor(before, this, "Mix With Color", QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid() || chosen == before) return;

    sl->fillColor2 = chosen;
    sl->regenerate();
    setSwatch(m_gradientColorBtn, chosen);
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<QColor>(
            m_project, m_layer->id(), before, chosen,
            [](Layer* l) { return static_cast<ShapeLayer*>(l)->fillColor2; },
            [](Layer* l, QColor val) {
                auto* s = static_cast<ShapeLayer*>(l);
                s->fillColor2 = val;
                s->regenerate();
            },
            "Change Gradient Color");
        m_undoStack->push(cmd);
    }
}

void PropertiesPanel::pushShapeGradientAngle() {
    if (m_updatingUi || !m_layer || !m_project || m_layer->type() != LayerType::Shape) return;
    auto* sl = static_cast<ShapeLayer*>(m_layer);

    double before = sl->gradientAngleDeg;
    double after = m_gradientAngle->value();
    if (before == after) return;

    sl->gradientAngleDeg = after;
    sl->regenerate();
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<double>(
            m_project, m_layer->id(), before, after,
            [](Layer* l) { return static_cast<ShapeLayer*>(l)->gradientAngleDeg; },
            [](Layer* l, double val) {
                auto* s = static_cast<ShapeLayer*>(l);
                s->gradientAngleDeg = val;
                s->regenerate();
            },
            "Change Gradient Angle");
        m_undoStack->push(cmd);
    }
}

void PropertiesPanel::pushShapeCornerRadius() {
    if (m_updatingUi || !m_layer || !m_project || m_layer->type() != LayerType::Shape) return;
    auto* sl = static_cast<ShapeLayer*>(m_layer);

    int before = sl->cornerRadius;
    int after = m_cornerRadius->value();
    if (before == after) return;

    sl->cornerRadius = after;
    sl->regenerate();
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<int>(
            m_project, m_layer->id(), before, after,
            [](Layer* l) { return static_cast<ShapeLayer*>(l)->cornerRadius; },
            [](Layer* l, int val) {
                auto* s = static_cast<ShapeLayer*>(l);
                s->cornerRadius = val;
                s->regenerate();
            },
            "Change Corner Radius");
        m_undoStack->push(cmd);
    }
}

void PropertiesPanel::pushShapeFillColor() {
    if (!m_layer || !m_project || m_layer->type() != LayerType::Shape) return;
    auto* sl = static_cast<ShapeLayer*>(m_layer);

    QColor before = sl->fillColor;
    QColor chosen = QColorDialog::getColor(before, this, "Fill Color", QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid() || chosen == before) return;

    sl->fillColor = chosen;
    sl->regenerate();
    setSwatch(m_fillColorBtn, chosen);
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<QColor>(
            m_project, m_layer->id(), before, chosen,
            [](Layer* l) { return static_cast<ShapeLayer*>(l)->fillColor; },
            [](Layer* l, QColor val) {
                auto* s = static_cast<ShapeLayer*>(l);
                s->fillColor = val;
                s->regenerate();
            },
            "Change Fill Color");
        m_undoStack->push(cmd);
    }
}

void PropertiesPanel::pushShapeStrokeColor() {
    if (!m_layer || !m_project || m_layer->type() != LayerType::Shape) return;
    auto* sl = static_cast<ShapeLayer*>(m_layer);

    QColor before = sl->strokeColor;
    QColor chosen = QColorDialog::getColor(before, this, "Stroke Color", QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid() || chosen == before) return;

    sl->strokeColor = chosen;
    sl->regenerate();
    setSwatch(m_strokeColorBtn, chosen);
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<QColor>(
            m_project, m_layer->id(), before, chosen,
            [](Layer* l) { return static_cast<ShapeLayer*>(l)->strokeColor; },
            [](Layer* l, QColor val) {
                auto* s = static_cast<ShapeLayer*>(l);
                s->strokeColor = val;
                s->regenerate();
            },
            "Change Stroke Color");
        m_undoStack->push(cmd);
    }
}

void PropertiesPanel::pushShapeStrokeWidth() {
    if (m_updatingUi || !m_layer || !m_project || m_layer->type() != LayerType::Shape) return;
    auto* sl = static_cast<ShapeLayer*>(m_layer);

    int before = sl->strokeWidth;
    int after = m_strokeWidth->value();
    if (before == after) return;

    sl->strokeWidth = after;
    sl->regenerate();
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<int>(
            m_project, m_layer->id(), before, after,
            [](Layer* l) { return static_cast<ShapeLayer*>(l)->strokeWidth; },
            [](Layer* l, int val) {
                auto* s = static_cast<ShapeLayer*>(l);
                s->strokeWidth = val;
                s->regenerate();
            },
            "Change Stroke Width");
        m_undoStack->push(cmd);
    }
}

// ---------------- Universal "mix color" (tint) ----------------

void PropertiesPanel::pushTintColor() {
    if (!m_layer || !m_project) return;

    QColor before = m_layer->tintColor;
    QColor chosen = QColorDialog::getColor(before, this, "Mix Color", QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid() || chosen == before) return;

    m_layer->tintColor = chosen;
    m_layer->applyTint();
    setSwatch(m_tintColorBtn, chosen);
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<QColor>(
            m_project, m_layer->id(), before, chosen,
            [](Layer* l) { return l->tintColor; },
            [](Layer* l, QColor val) {
                l->tintColor = val;
                l->applyTint();
            },
            "Change Mix Color");
        m_undoStack->push(cmd);
    }
}

void PropertiesPanel::pushTintAmount() {
    if (m_updatingUi || !m_layer || !m_project) return;

    double before = m_layer->tintAmount;
    double after = m_tintAmount->value() / 100.0;
    if (before == after) return;

    m_layer->tintAmount = after; // live preview
    m_layer->applyTint();
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<double>(
            m_project, m_layer->id(), before, after,
            [](Layer* l) { return l->tintAmount; },
            [](Layer* l, double val) {
                l->tintAmount = val;
                l->applyTint();
            },
            "Change Mix Amount");
        m_undoStack->push(cmd);
    }
}

// ---------------- Background removal (raster layers) ----------------

void PropertiesPanel::pushBgRemovalColor() {
    QColor chosen = QColorDialog::getColor(m_bgPickedColor, this, "Background Color", QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid()) return;
    m_bgPickedColor = chosen;
    setSwatch(m_bgColorBtn, chosen);
}

void PropertiesPanel::pushBgRemovalTolerance(int value) {
    m_bgToleranceLabel->setText(QString("%1%").arg(value));
}

void PropertiesPanel::runBackgroundRemoval() {
    if (!m_layer || !m_project || m_layer->type() != LayerType::Raster) return;

    QImage before = m_layer->content;
    ImageBuffer result = BackgroundRemoval::removeByColor(ImageBuffer(before), m_bgPickedColor, m_bgTolerance->value());
    if (result.isNull()) return;
    QImage after = result.image();

    m_layer->content = after;
    m_layer->applyTint();
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<QImage>(
            m_project, m_layer->id(), before, after,
            [](Layer* l) { return l->content; },
            [](Layer* l, QImage val) {
                l->content = val;
                l->applyTint();
            },
            "Remove Background");
        m_undoStack->push(cmd);
    }
}

// ---------------- Sharpen (OpenCV) ----------------

void PropertiesPanel::pushSharpenAmount(int value) {
    m_sharpenAmountLabel->setText(QString::number(value / 100.0, 'f', 2));
}

void PropertiesPanel::pushSharpenRadius() {
    // Nothing to push live - radius is only read when "Apply Sharpen" runs.
}

void PropertiesPanel::pushSharpenThreshold(int value) {
    m_sharpenThresholdLabel->setText(QString::number(value));
}

void PropertiesPanel::toggleSharpenSmartMode(bool checked) {
    m_sharpenDenoise->setEnabled(checked);
}

void PropertiesPanel::runSharpen() {
    if (!m_layer || !m_project || m_layer->content.isNull()) return;

    double amount = m_sharpenAmount->value() / 100.0;
    double radius = m_sharpenRadius->value();
    int threshold = m_sharpenThreshold->value();

    QImage before = m_layer->content;
    ImageBuffer result;
    if (m_sharpenSmartMode->isChecked()) {
        double strength = m_sharpenDenoise->value() / 100.0;
        result = CvSharpen::smartSharpen(ImageBuffer(before), amount, radius, strength);
    } else {
        result = CvSharpen::unsharpMask(ImageBuffer(before), amount, radius, threshold);
    }
    if (result.isNull()) return;
    QImage after = result.image();

    m_layer->content = after;
    m_layer->applyTint();
    emit requestCanvasUpdate();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<QImage>(
            m_project, m_layer->id(), before, after,
            [](Layer* l) { return l->content; },
            [](Layer* l, QImage val) {
                l->content = val;
                l->applyTint();
            },
            "Sharpen");
        m_undoStack->push(cmd);
    }
}
