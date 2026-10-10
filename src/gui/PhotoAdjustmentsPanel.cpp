#include "PhotoAdjustmentsPanel.h"
#include "../project/Commands.h"
#include "../engine/PresetLibrary.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QMessageBox>
#include <QDialog>
#include <QListWidget>
#include <algorithm>
#include <cmath>
#include <QTimer>

// App-wide static clipboard — shared across all panel instances
bool PhotoAdjustmentsPanel::s_clipboardHasData = false;
ToneParameters PhotoAdjustmentsPanel::s_clipboard;

PhotoAdjustmentsPanel::PhotoAdjustmentsPanel(QWidget* parent) : QWidget(parent) {
    buildUi();
    setupDarkStyle();
    setLayer(nullptr);
}

void PhotoAdjustmentsPanel::setupDarkStyle() {
    setStyleSheet(R"(
        QWidget {
            background-color: #1e1e1e;
            color: #d1d5db;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            font-size: 12px;
        }
        QScrollArea {
            border: none;
            background-color: #1e1e1e;
        }
        QScrollBar:vertical {
            border: none;
            background: #1e1e1e;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #3f3f46;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #52525b;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QPushButton {
            background-color: #2e2e2e;
            color: #e4e4e7;
            border: 1px solid #3f3f46;
            border-radius: 5px;
            padding: 5px 10px;
            font-size: 11px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #3b3b3b;
            border-color: #52525b;
        }
        QPushButton:pressed {
            background-color: #222222;
        }
        QPushButton:checked {
            background-color: #2563eb;
            color: #ffffff;
            border-color: #3b82f6;
            font-weight: bold;
        }
        QComboBox {
            background-color: #27272a;
            color: #f4f4f5;
            border: 1px solid #3f3f46;
            border-radius: 5px;
            padding: 4px 8px;
            font-size: 12px;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #27272a;
            color: #f4f4f5;
            selection-background-color: #3b82f6;
            selection-color: #ffffff;
            border: 1px solid #3f3f46;
            border-radius: 4px;
        }
        QSlider::groove:horizontal {
            height: 4px;
            background: #3f3f46;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: #71717a;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #f4f4f5;
            border: 2px solid #27272a;
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border-radius: 7px;
        }
        QSlider::handle:horizontal:hover {
            background: #ffffff;
            border-color: #3b82f6;
        }
        QCheckBox {
            color: #d4d4d8;
            font-size: 11px;
        }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            border-radius: 3px;
            border: 1px solid #52525b;
            background: #27272a;
        }
        QCheckBox::indicator:checked {
            background: #3b82f6;
            border-color: #3b82f6;
        }
    )");
}

PhotoAdjustmentsPanel::SliderRow PhotoAdjustmentsPanel::createSliderRow(
    QWidget* parent, const QString& title, int min, int max, int defaultVal,
    double scale, int decimals, const QString& /*unit*/)
{
    SliderRow row;
    row.scale = scale;
    row.decimals = decimals;

    row.container = new QWidget(parent);
    auto* vLayout = new QVBoxLayout(row.container);
    vLayout->setContentsMargins(0, 3, 0, 3);
    vLayout->setSpacing(2);

    auto* topLayout = new QHBoxLayout();
    topLayout->setContentsMargins(0, 0, 0, 0);

    row.titleLabel = new QLabel(title, row.container);
    row.titleLabel->setStyleSheet("color: #a1a1aa; font-size: 11px; font-weight: 500;");

    row.valueLabel = new QLabel("0", row.container);
    row.valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row.valueLabel->setStyleSheet("color: #e4e4e7; font-size: 11px; font-weight: 600; font-family: monospace;");

    topLayout->addWidget(row.titleLabel);
    topLayout->addStretch(1);
    topLayout->addWidget(row.valueLabel);
    vLayout->addLayout(topLayout);

    row.slider = new QSlider(Qt::Horizontal, row.container);
    row.slider->setRange(min, max);
    row.slider->setValue(defaultVal);
    vLayout->addWidget(row.slider);

    updateValueLabel(row, defaultVal);
    return row;
}

void PhotoAdjustmentsPanel::updateValueLabel(SliderRow& row, int rawValue) {
    double realVal = rawValue * row.scale;
    QString strVal;

    if (row.decimals > 0) {
        strVal = QString::number(realVal, 'f', row.decimals);
        strVal.replace('.', ','); // format as "+ 0,29" or "- 1,50"
    } else {
        strVal = QString::number(static_cast<int>(std::round(realVal)));
    }

    if (realVal > 1e-4) {
        row.valueLabel->setText(QString("+ %1").arg(strVal));
    } else if (realVal < -1e-4) {
        row.valueLabel->setText(QString("- %1").arg(strVal.remove('-')));
    } else {
        row.valueLabel->setText("0");
    }
}

void PhotoAdjustmentsPanel::connectSliderRow(
    SliderRow& row, std::function<void(double)> onLiveChange, std::function<void()> onCommit)
{
    connect(row.slider, &QSlider::sliderPressed, this, [this]() {
        if (m_layer) m_savedBeforeParams = m_layer->adjustments;
    });

    connect(row.slider, &QSlider::valueChanged, this, [this, &row, onLiveChange](int v) {
        updateValueLabel(row, v);
        if (!m_updatingUi && m_layer) {
            onLiveChange(v * row.scale);
            applyLiveAdjustments();
        }
    });

    connect(row.slider, &QSlider::sliderReleased, this, [this, onCommit]() {
        if (!m_updatingUi && m_layer) {
            onCommit();
        }
    });
}

QWidget* PhotoAdjustmentsPanel::createCollapsibleSection(const QString& title, QWidget* contentWidget, bool expanded) {
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(2);

    auto* toggleBtn = new QPushButton(container);
    toggleBtn->setCheckable(true);
    toggleBtn->setChecked(expanded);
    toggleBtn->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            border: none;
            text-align: left;
            padding: 6px 2px;
            font-size: 12px;
            font-weight: 700;
            color: #f4f4f5;
        }
        QPushButton:hover {
            color: #38bdf8;
        }
    )");

    auto updateArrow = [toggleBtn, title](bool checked) {
        toggleBtn->setText(QString("%1  %2").arg(checked ? "▾" : "▸", title));
    };
    updateArrow(expanded);

    contentWidget->setVisible(expanded);
    connect(toggleBtn, &QPushButton::toggled, this, [contentWidget, updateArrow](bool checked) {
        contentWidget->setVisible(checked);
        updateArrow(checked);
    });

    layout->addWidget(toggleBtn);
    layout->addWidget(contentWidget);
    return container;
}

void PhotoAdjustmentsPanel::buildUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(6, 6, 6, 6);
    outerLayout->setSpacing(4);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(scrollArea);
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(2, 2, 2, 2);
    root->setSpacing(8);

    // 1. Histogram Widget at the top
    m_histogram = new HistogramWidget(content);
    root->addWidget(m_histogram);

    // 2. Header: Title "Chỉnh sửa" + Quick Action Buttons
    auto* headerWidget = new QWidget(content);
    auto* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 2, 0, 4);
    headerLayout->setSpacing(6);

    auto* titleLabel = new QLabel("<b>Chỉnh sửa</b>", headerWidget);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: 700; color: #f4f4f5;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch(1);

    m_btnAuto = new QPushButton("Tự động", headerWidget);
    m_btnAuto->setToolTip("Tự động tính toán thông số ánh sáng tối ưu nhất từ biểu đồ Histogram");
    connect(m_btnAuto, &QPushButton::clicked, this, &PhotoAdjustmentsPanel::onAutoToneClicked);
    headerLayout->addWidget(m_btnAuto);

    m_btnBw = new QPushButton("Trắng đen", headerWidget);
    m_btnBw->setCheckable(true);
    m_btnBw->setToolTip("Chuyển sang chế độ ảnh đen trắng nghệ thuật");
    connect(m_btnBw, &QPushButton::toggled, this, &PhotoAdjustmentsPanel::onBwModeToggled);
    headerLayout->addWidget(m_btnBw);

    m_btnHdr = new QPushButton("HDR", headerWidget);
    m_btnHdr->setCheckable(true);
    m_btnHdr->setToolTip("Bật chế độ HDR tăng cường dải nhạy sáng động");
    connect(m_btnHdr, &QPushButton::toggled, this, &PhotoAdjustmentsPanel::onHdrModeToggled);
    headerLayout->addWidget(m_btnHdr);

    m_btnInfo = new QPushButton("ⓘ", headerWidget);
    m_btnInfo->setFixedWidth(26);
    m_btnInfo->setToolTip("Thông tin bộ công cụ chỉnh sửa ảnh");
    connect(m_btnInfo, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, "Chỉnh sửa hình ảnh",
            "<b>Bộ công cụ Chỉnh sửa hình ảnh & Đổi màu cao cấp</b><br><br>"
            "• <b>Tự động</b>: Cân bằng dải sáng tự động dựa trên Histogram.<br>"
            "• <b>Cấu hình / Presets</b>: Bộ sưu tập phong cách màu chuyên nghiệp.<br>"
            "• <b>Ánh sáng & Màu sắc</b>: Chỉnh sửa không phá huỷ (non-destructive) tức thì.<br>"
            "• <b>Đường cong điểm (Point Curve)</b>: Chỉnh độ cong chi tiết 4 kênh RGB.<br>"
            "• Click đúp vào thanh trượt để khôi phục về 0.");
    });
    headerLayout->addWidget(m_btnInfo);
    root->addWidget(headerWidget);

    // Separator line
    auto addDivider = [&root, content]() {
        auto* line = new QFrame(content);
        line->setFrameShape(QFrame::HLine);
        line->setStyleSheet("background-color: #27272a; border: none; max-height: 1px;");
        root->addWidget(line);
    };
    addDivider();

    // 3. Profiles / Presets Section
    auto* profileContainer = new QWidget(content);
    auto* profileLayout = new QVBoxLayout(profileContainer);
    profileLayout->setContentsMargins(0, 2, 0, 4);
    profileLayout->setSpacing(6);

    auto* pRow1 = new QHBoxLayout();
    auto* pLabel = new QLabel("Cấu hình", profileContainer);
    pLabel->setStyleSheet("color: #e4e4e7; font-weight: 600; font-size: 12px;");
    pRow1->addWidget(pLabel);

    m_profileCombo = new QComboBox(profileContainer);
    m_profileCombo->addItems(PresetLibrary::presetNames());
    connect(m_profileCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &PhotoAdjustmentsPanel::onProfileSelected);
    pRow1->addWidget(m_profileCombo, 1);

    m_btnProfileGrid = new QPushButton("⊞", profileContainer);
    m_btnProfileGrid->setFixedWidth(28);
    m_btnProfileGrid->setToolTip("Xem danh sách cấu hình / Presets");
    connect(m_btnProfileGrid, &QPushButton::clicked, this, [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle("Chọn cấu hình màu (Presets)");
        dlg.setMinimumSize(320, 400);
        auto* dlgLayout = new QVBoxLayout(&dlg);
        auto* list = new QListWidget(&dlg);
        list->addItems(PresetLibrary::presetNames());
        list->setCurrentRow(m_profileCombo->currentIndex());
        dlgLayout->addWidget(list);

        auto* btnApply = new QPushButton("Áp dụng cấu hình", &dlg);
        dlgLayout->addWidget(btnApply);
        connect(btnApply, &QPushButton::clicked, [&]() {
            if (list->currentRow() >= 0) {
                m_profileCombo->setCurrentIndex(list->currentRow());
            }
            dlg.accept();
        });
        dlg.exec();
    });
    pRow1->addWidget(m_btnProfileGrid);
    profileLayout->addLayout(pRow1);

    // Profile Amount Slider Row
    auto* pRow2 = new QHBoxLayout();
    auto* pAmountLabelTitle = new QLabel("Tỷ lệ", profileContainer);
    pAmountLabelTitle->setStyleSheet("color: #a1a1aa; font-size: 11px;");
    m_profileAmountLabel = new QLabel("100", profileContainer);
    m_profileAmountLabel->setStyleSheet("color: #f4f4f5; font-size: 11px; font-weight: 600;");
    pRow2->addWidget(pAmountLabelTitle);
    pRow2->addStretch(1);
    pRow2->addWidget(m_profileAmountLabel);
    profileLayout->addLayout(pRow2);

    m_profileAmountSlider = new QSlider(Qt::Horizontal, profileContainer);
    m_profileAmountSlider->setRange(0, 200);
    m_profileAmountSlider->setValue(100);
    connect(m_profileAmountSlider, &QSlider::valueChanged, this, &PhotoAdjustmentsPanel::onProfileAmountChanged);
    profileLayout->addWidget(m_profileAmountSlider);

    root->addWidget(profileContainer);
    addDivider();

    // 4. Section 1: Ánh sáng (Light)
    auto* lightContent = new QWidget(content);
    auto* lightLayout = new QVBoxLayout(lightContent);
    lightLayout->setContentsMargins(0, 0, 0, 0);
    lightLayout->setSpacing(4);

    m_rowExposure = createSliderRow(lightContent, "Độ phơi sáng", -500, 500, 0, 0.01, 2);
    m_rowContrast = createSliderRow(lightContent, "Độ tương phản", -100, 100, 0, 1.0, 0);
    m_rowHighlights = createSliderRow(lightContent, "Vùng sáng", -100, 100, 0, 1.0, 0);
    m_rowShadows = createSliderRow(lightContent, "Vùng tối", -100, 100, 0, 1.0, 0);
    m_rowWhites = createSliderRow(lightContent, "Màu trắng", -100, 100, 0, 1.0, 0);
    m_rowBlacks = createSliderRow(lightContent, "Màu đen", -100, 100, 0, 1.0, 0);

    for (auto& r : {&m_rowExposure, &m_rowContrast, &m_rowHighlights, &m_rowShadows, &m_rowWhites, &m_rowBlacks}) {
        lightLayout->addWidget(r->container);
    }

    connectSliderRow(m_rowExposure, [this](double val) { m_layer->adjustments.exposure = val; }, [this]() { pushUndoCommand("Change Exposure"); });
    connectSliderRow(m_rowContrast, [this](double val) { m_layer->adjustments.contrast = val; }, [this]() { pushUndoCommand("Change Contrast"); });
    connectSliderRow(m_rowHighlights, [this](double val) { m_layer->adjustments.highlights = val; }, [this]() { pushUndoCommand("Change Highlights"); });
    connectSliderRow(m_rowShadows, [this](double val) { m_layer->adjustments.shadows = val; }, [this]() { pushUndoCommand("Change Shadows"); });
    connectSliderRow(m_rowWhites, [this](double val) { m_layer->adjustments.whites = val; }, [this]() { pushUndoCommand("Change Whites"); });
    connectSliderRow(m_rowBlacks, [this](double val) { m_layer->adjustments.blacks = val; }, [this]() { pushUndoCommand("Change Blacks"); });

    root->addWidget(createCollapsibleSection("Ánh sáng", lightContent, true));
    addDivider();

    // 5. Section 2: Màu sắc (Color)
    auto* colorContent = new QWidget(content);
    auto* colorLayout = new QVBoxLayout(colorContent);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(4);

    m_rowTemp = createSliderRow(colorContent, "Nhiệt độ màu", -100, 100, 0, 1.0, 0);
    m_rowTint = createSliderRow(colorContent, "Sắc thái", -100, 100, 0, 1.0, 0);
    m_rowVibrance = createSliderRow(colorContent, "Độ đậm màu", -100, 100, 0, 1.0, 0);
    m_rowSaturation = createSliderRow(colorContent, "Độ bão hoà", -100, 100, 0, 1.0, 0);

    for (auto& r : {&m_rowTemp, &m_rowTint, &m_rowVibrance, &m_rowSaturation}) {
        colorLayout->addWidget(r->container);
    }

    connectSliderRow(m_rowTemp, [this](double val) { m_layer->adjustments.temp = val; }, [this]() { pushUndoCommand("Change Temperature"); });
    connectSliderRow(m_rowTint, [this](double val) { m_layer->adjustments.tint = val; }, [this]() { pushUndoCommand("Change Tint"); });
    connectSliderRow(m_rowVibrance, [this](double val) { m_layer->adjustments.vibrance = val; }, [this]() { pushUndoCommand("Change Vibrance"); });
    connectSliderRow(m_rowSaturation, [this](double val) { m_layer->adjustments.saturation = val; }, [this]() { pushUndoCommand("Change Saturation"); });

    root->addWidget(createCollapsibleSection("Màu sắc", colorContent, true));
    addDivider();

    // 6. Section 3: Chi tiết & Hiệu ứng (Effects & Detail)
    auto* effectContent = new QWidget(content);
    auto* effectLayout = new QVBoxLayout(effectContent);
    effectLayout->setContentsMargins(0, 0, 0, 0);
    effectLayout->setSpacing(4);

    m_rowClarity = createSliderRow(effectContent, "Độ rõ nét", -100, 100, 0, 1.0, 0);
    m_rowDehaze = createSliderRow(effectContent, "Khử mờ sương", -100, 100, 0, 1.0, 0);
    m_rowVignette = createSliderRow(effectContent, "Viền tối", -100, 100, 0, 1.0, 0);

    for (auto& r : {&m_rowClarity, &m_rowDehaze, &m_rowVignette}) {
        effectLayout->addWidget(r->container);
    }

    connectSliderRow(m_rowClarity, [this](double val) { m_layer->adjustments.clarity = val; }, [this]() { pushUndoCommand("Change Clarity"); });
    connectSliderRow(m_rowDehaze, [this](double val) { m_layer->adjustments.dehaze = val; }, [this]() { pushUndoCommand("Change Dehaze"); });
    connectSliderRow(m_rowVignette, [this](double val) { m_layer->adjustments.vignette = val; }, [this]() { pushUndoCommand("Change Vignette"); });

    root->addWidget(createCollapsibleSection("Chi tiết & Hiệu ứng", effectContent, false));
    addDivider();

    // 7. Section 4: Đường cong điểm (Point Curve)
    auto* curveContent = new QWidget(content);
    auto* curveLayout = new QVBoxLayout(curveContent);
    curveLayout->setContentsMargins(0, 0, 0, 0);

    m_curveWidget = new CurveWidget(curveContent);
    connect(m_curveWidget, &CurveWidget::curvesChanged, this, [this]() {
        if (!m_updatingUi && m_layer) {
            ToneParameters before = m_layer->adjustments;
            m_layer->adjustments.curveRgb = m_curveWidget->curveRgb();
            m_layer->adjustments.curveRed = m_curveWidget->curveRed();
            m_layer->adjustments.curveGreen = m_curveWidget->curveGreen();
            m_layer->adjustments.curveBlue = m_curveWidget->curveBlue();
            applyLiveAdjustments();
            if (m_undoStack) {
                m_undoStack->push(new SetToneAdjustmentsCommand(m_project, m_layer->id(), before, m_layer->adjustments, "Change Point Curve"));
            }
        }
    });
    curveLayout->addWidget(m_curveWidget);

    root->addWidget(createCollapsibleSection("Đường cong điểm", curveContent, true));
    addDivider();

    // 8. Footer Action Bar
    auto* footer = new QWidget(content);
    auto* footerLayout = new QVBoxLayout(footer);
    footerLayout->setContentsMargins(0, 6, 0, 6);
    footerLayout->setSpacing(6);

    // Row 1: Copy / Paste
    auto* copyPasteRow = new QHBoxLayout();

    m_btnCopy = new QPushButton("⎘  Sao chép thông số", footer);
    m_btnCopy->setToolTip("Sao chép tất cả thông số chỉnh sửa của layer này\n"
                          "để dán sang layer khác (Ctrl+Shift+C)");
    m_btnCopy->setStyleSheet(
        "QPushButton { background-color: #27272a; color: #60a5fa; border: 1px solid #3f3f46; "
        "border-radius: 5px; padding: 5px 10px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background-color: #1d4ed8; color: #fff; border-color: #3b82f6; }"
        "QPushButton:pressed { background-color: #1e40af; }"
    );
    connect(m_btnCopy, &QPushButton::clicked, this, &PhotoAdjustmentsPanel::onCopyAdjustments);
    copyPasteRow->addWidget(m_btnCopy);

    m_btnPaste = new QPushButton("⎙  Dán thông số", footer);
    m_btnPaste->setToolTip("Dán thông số đã sao chép vào layer này (Ctrl+Shift+V)");
    m_btnPaste->setEnabled(false); // disabled until clipboard has data
    m_btnPaste->setStyleSheet(
        "QPushButton { background-color: #27272a; color: #a3e635; border: 1px solid #3f3f46; "
        "border-radius: 5px; padding: 5px 10px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background-color: #3f6212; color: #fff; border-color: #84cc16; }"
        "QPushButton:pressed { background-color: #365314; }"
        "QPushButton:disabled { color: #52525b; border-color: #3f3f46; }"
    );
    connect(m_btnPaste, &QPushButton::clicked, this, &PhotoAdjustmentsPanel::onPasteAdjustments);
    copyPasteRow->addWidget(m_btnPaste);

    footerLayout->addLayout(copyPasteRow);

    // Row 2: Reset / Bake
    auto* btnRow = new QHBoxLayout();
    m_btnResetAll = new QPushButton("Đặt lại tất cả", footer);
    connect(m_btnResetAll, &QPushButton::clicked, this, &PhotoAdjustmentsPanel::onResetAllClicked);
    btnRow->addWidget(m_btnResetAll);

    m_btnBake = new QPushButton("Áp dụng vĩnh viễn (Bake)", footer);
    m_btnBake->setStyleSheet("background-color: #15803d; color: white; font-weight: bold; border-color: #16a34a;");
    connect(m_btnBake, &QPushButton::clicked, this, &PhotoAdjustmentsPanel::onBakeClicked);
    btnRow->addWidget(m_btnBake);

    footerLayout->addLayout(btnRow);

    m_chkLivePreview = new QCheckBox("Bật xem trước trực tiếp", footer);
    m_chkLivePreview->setChecked(true);
    connect(m_chkLivePreview, &QCheckBox::toggled, this, [this](bool) {
        if (m_layer) {
            m_layer->applyAdjustments();
            emit requestCanvasUpdate();
            updateHistogram();
        }
    });
    footerLayout->addWidget(m_chkLivePreview);

    root->addWidget(footer);
    root->addStretch(1);

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
}

void PhotoAdjustmentsPanel::setLayer(Layer* layer) {
    m_layer = layer;
    bool enabled = (layer != nullptr);

    setEnabled(enabled);
    // Paste button: enabled only when clipboard has data AND a layer is selected
    m_btnPaste->setEnabled(enabled && s_clipboardHasData);

    if (!enabled) {
        m_histogram->clear();
    } else {
        refreshFromLayer();
        updateHistogram();
    }
}

void PhotoAdjustmentsPanel::refreshFromLayer() {
    if (!m_layer) return;

    m_updatingUi = true;
    const ToneParameters& p = m_layer->adjustments;

    m_btnBw->setChecked(p.bwMode);
    m_btnHdr->setChecked(p.hdrMode);

    int profileIdx = m_profileCombo->findText(p.activePreset);
    if (profileIdx >= 0) m_profileCombo->setCurrentIndex(profileIdx);
    else m_profileCombo->setCurrentIndex(0);

    m_profileAmountSlider->setValue(static_cast<int>(std::round(p.presetAmount)));
    m_profileAmountLabel->setText(QString::number(static_cast<int>(std::round(p.presetAmount))));

    // Light
    m_rowExposure.slider->setValue(static_cast<int>(std::round(p.exposure / m_rowExposure.scale)));
    updateValueLabel(m_rowExposure, m_rowExposure.slider->value());

    m_rowContrast.slider->setValue(static_cast<int>(std::round(p.contrast)));
    updateValueLabel(m_rowContrast, m_rowContrast.slider->value());

    m_rowHighlights.slider->setValue(static_cast<int>(std::round(p.highlights)));
    updateValueLabel(m_rowHighlights, m_rowHighlights.slider->value());

    m_rowShadows.slider->setValue(static_cast<int>(std::round(p.shadows)));
    updateValueLabel(m_rowShadows, m_rowShadows.slider->value());

    m_rowWhites.slider->setValue(static_cast<int>(std::round(p.whites)));
    updateValueLabel(m_rowWhites, m_rowWhites.slider->value());

    m_rowBlacks.slider->setValue(static_cast<int>(std::round(p.blacks)));
    updateValueLabel(m_rowBlacks, m_rowBlacks.slider->value());

    // Color
    m_rowTemp.slider->setValue(static_cast<int>(std::round(p.temp)));
    updateValueLabel(m_rowTemp, m_rowTemp.slider->value());

    m_rowTint.slider->setValue(static_cast<int>(std::round(p.tint)));
    updateValueLabel(m_rowTint, m_rowTint.slider->value());

    m_rowVibrance.slider->setValue(static_cast<int>(std::round(p.vibrance)));
    updateValueLabel(m_rowVibrance, m_rowVibrance.slider->value());

    m_rowSaturation.slider->setValue(static_cast<int>(std::round(p.saturation)));
    updateValueLabel(m_rowSaturation, m_rowSaturation.slider->value());

    // Effects
    m_rowClarity.slider->setValue(static_cast<int>(std::round(p.clarity)));
    updateValueLabel(m_rowClarity, m_rowClarity.slider->value());

    m_rowDehaze.slider->setValue(static_cast<int>(std::round(p.dehaze)));
    updateValueLabel(m_rowDehaze, m_rowDehaze.slider->value());

    m_rowVignette.slider->setValue(static_cast<int>(std::round(p.vignette)));
    updateValueLabel(m_rowVignette, m_rowVignette.slider->value());

    // Curves
    m_curveWidget->setCurves(p.curveRgb, p.curveRed, p.curveGreen, p.curveBlue);

    m_updatingUi = false;
}

void PhotoAdjustmentsPanel::updateHistogram() {
    if (!m_layer || m_layer->content.isNull()) {
        m_histogram->clear();
        return;
    }
    const QImage& disp = m_layer->displayContent();
    HistogramData hist = PhotoEngine::computeHistogram(ImageBuffer(disp));
    m_histogram->setHistogramData(hist);
}

void PhotoAdjustmentsPanel::applyLiveAdjustments() {
    if (!m_layer) return;
    if (m_chkLivePreview->isChecked()) {
        m_layer->applyAdjustments();
    }
    emit requestCanvasUpdate();
    updateHistogram();
}

void PhotoAdjustmentsPanel::pushUndoCommand(const QString& text) {
    if (!m_layer || !m_undoStack) return;
    ToneParameters current = m_layer->adjustments;
    if (current != m_savedBeforeParams) {
        m_undoStack->push(new SetToneAdjustmentsCommand(m_project, m_layer->id(), m_savedBeforeParams, current, text));
        m_savedBeforeParams = current;
    }
}

void PhotoAdjustmentsPanel::onAutoToneClicked() {
    if (!m_layer || m_layer->content.isNull()) return;

    ToneParameters before = m_layer->adjustments;
    ToneParameters autoParams = PhotoEngine::computeAutoTone(ImageBuffer(m_layer->content));

    m_layer->adjustments.exposure = autoParams.exposure;
    m_layer->adjustments.contrast = autoParams.contrast;
    m_layer->adjustments.highlights = autoParams.highlights;
    m_layer->adjustments.shadows = autoParams.shadows;
    m_layer->adjustments.whites = autoParams.whites;
    m_layer->adjustments.blacks = autoParams.blacks;
    m_layer->adjustments.vibrance = autoParams.vibrance;
    m_layer->adjustments.saturation = autoParams.saturation;

    refreshFromLayer();
    applyLiveAdjustments();

    if (m_undoStack) {
        m_undoStack->push(new SetToneAdjustmentsCommand(m_project, m_layer->id(), before, m_layer->adjustments, "Auto Tone"));
    }
}

void PhotoAdjustmentsPanel::onBwModeToggled(bool checked) {
    if (m_updatingUi || !m_layer) return;
    ToneParameters before = m_layer->adjustments;
    m_layer->adjustments.bwMode = checked;
    applyLiveAdjustments();
    if (m_undoStack) {
        m_undoStack->push(new SetToneAdjustmentsCommand(m_project, m_layer->id(), before, m_layer->adjustments, "Toggle Black & White"));
    }
}

void PhotoAdjustmentsPanel::onHdrModeToggled(bool checked) {
    if (m_updatingUi || !m_layer) return;
    ToneParameters before = m_layer->adjustments;
    m_layer->adjustments.hdrMode = checked;
    applyLiveAdjustments();
    if (m_undoStack) {
        m_undoStack->push(new SetToneAdjustmentsCommand(m_project, m_layer->id(), before, m_layer->adjustments, "Toggle HDR"));
    }
}

void PhotoAdjustmentsPanel::onProfileSelected(int index) {
    if (m_updatingUi || !m_layer) return;
    QString presetName = m_profileCombo->itemText(index);
    ToneParameters before = m_layer->adjustments;

    ToneParameters newParams = PresetLibrary::applyPresetWithAmount(presetName, m_profileAmountSlider->value());
    m_layer->adjustments = newParams;

    refreshFromLayer();
    applyLiveAdjustments();

    if (m_undoStack) {
        m_undoStack->push(new SetToneAdjustmentsCommand(m_project, m_layer->id(), before, m_layer->adjustments, "Apply Preset " + presetName));
    }
}

void PhotoAdjustmentsPanel::onProfileAmountChanged(int value) {
    m_profileAmountLabel->setText(QString::number(value));
    if (m_updatingUi || !m_layer) return;

    QString presetName = m_profileCombo->currentText();
    m_layer->adjustments = PresetLibrary::applyPresetWithAmount(presetName, value);

    // Refresh UI sliders without resetting combo
    m_updatingUi = true;
    m_rowExposure.slider->setValue(static_cast<int>(std::round(m_layer->adjustments.exposure / m_rowExposure.scale)));
    m_rowContrast.slider->setValue(static_cast<int>(std::round(m_layer->adjustments.contrast)));
    m_rowHighlights.slider->setValue(static_cast<int>(std::round(m_layer->adjustments.highlights)));
    m_rowShadows.slider->setValue(static_cast<int>(std::round(m_layer->adjustments.shadows)));
    m_rowWhites.slider->setValue(static_cast<int>(std::round(m_layer->adjustments.whites)));
    m_rowBlacks.slider->setValue(static_cast<int>(std::round(m_layer->adjustments.blacks)));
    m_rowTemp.slider->setValue(static_cast<int>(std::round(m_layer->adjustments.temp)));
    m_rowTint.slider->setValue(static_cast<int>(std::round(m_layer->adjustments.tint)));
    m_rowVibrance.slider->setValue(static_cast<int>(std::round(m_layer->adjustments.vibrance)));
    m_rowSaturation.slider->setValue(static_cast<int>(std::round(m_layer->adjustments.saturation)));

    updateValueLabel(m_rowExposure, m_rowExposure.slider->value());
    updateValueLabel(m_rowContrast, m_rowContrast.slider->value());
    updateValueLabel(m_rowHighlights, m_rowHighlights.slider->value());
    updateValueLabel(m_rowShadows, m_rowShadows.slider->value());
    updateValueLabel(m_rowWhites, m_rowWhites.slider->value());
    updateValueLabel(m_rowBlacks, m_rowBlacks.slider->value());
    updateValueLabel(m_rowTemp, m_rowTemp.slider->value());
    updateValueLabel(m_rowTint, m_rowTint.slider->value());
    updateValueLabel(m_rowVibrance, m_rowVibrance.slider->value());
    updateValueLabel(m_rowSaturation, m_rowSaturation.slider->value());
    m_updatingUi = false;

    applyLiveAdjustments();
}

void PhotoAdjustmentsPanel::onResetAllClicked() {
    if (!m_layer) return;
    ToneParameters before = m_layer->adjustments;
    m_layer->adjustments.reset();
    refreshFromLayer();
    applyLiveAdjustments();
    if (m_undoStack) {
        m_undoStack->push(new SetToneAdjustmentsCommand(m_project, m_layer->id(), before, m_layer->adjustments, "Reset All Adjustments"));
    }
}

void PhotoAdjustmentsPanel::onBakeClicked() {
    if (!m_layer || m_layer->adjustments.isIdentity()) return;

    QImage before = m_layer->content;
    m_layer->bakeAdjustments();
    QImage after = m_layer->content;

    refreshFromLayer();
    applyLiveAdjustments();

    if (m_undoStack) {
        auto* cmd = new SetLayerPropertyCommand<QImage>(
            m_project, m_layer->id(), before, after,
            [](Layer* l) { return l->content; },
            [](Layer* l, QImage val) {
                l->content = val;
                l->applyAdjustments();
            },
            "Bake Photo Adjustments");
        m_undoStack->push(cmd);
    }
}

void PhotoAdjustmentsPanel::onCopyAdjustments() {
    if (!m_layer) return;

    s_clipboard = m_layer->adjustments;
    s_clipboardHasData = true;

    // Update paste button state
    m_btnPaste->setEnabled(true);

    // Visual feedback: briefly highlight Copy button
    m_btnCopy->setText("✓  Đã sao chép!");
    m_btnCopy->setStyleSheet(
        "QPushButton { background-color: #1d4ed8; color: #fff; border: 1px solid #3b82f6; "
        "border-radius: 5px; padding: 5px 10px; font-size: 11px; font-weight: 600; }"
    );
    // Restore text after 1.5s
    QTimer::singleShot(1500, this, [this]() {
        m_btnCopy->setText("⎘  Sao chép thông số");
        m_btnCopy->setStyleSheet(
            "QPushButton { background-color: #27272a; color: #60a5fa; border: 1px solid #3f3f46; "
            "border-radius: 5px; padding: 5px 10px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background-color: #1d4ed8; color: #fff; border-color: #3b82f6; }"
            "QPushButton:pressed { background-color: #1e40af; }"
        );
    });
}

void PhotoAdjustmentsPanel::onPasteAdjustments() {
    if (!m_layer || !s_clipboardHasData) return;
    if (s_clipboard == m_layer->adjustments) return; // nothing to do

    ToneParameters before = m_layer->adjustments;
    m_layer->adjustments = s_clipboard;

    refreshFromLayer();
    applyLiveAdjustments();

    if (m_undoStack) {
        m_undoStack->push(new SetToneAdjustmentsCommand(
            m_project, m_layer->id(), before, m_layer->adjustments,
            "Paste Photo Adjustments"));
    }

    // Visual feedback
    m_btnPaste->setText("✓  Đã dán!");
    QTimer::singleShot(1500, this, [this]() {
        m_btnPaste->setText("⎙  Dán thông số");
    });
}
