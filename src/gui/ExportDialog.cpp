#include "ExportDialog.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <algorithm>

ExportDialog::ExportDialog(QSize original, QWidget* parent) : QDialog(parent), m_original(original.expandedTo(QSize(1, 1))) {
    setWindowTitle("Export Image");
    setObjectName("exportDialog");
    auto* form = new QFormLayout(this);
    m_format = new QComboBox(this);
    m_format->setObjectName("exportFormat");
    m_format->addItem("PNG (lossless)", QByteArray("png"));
    m_format->addItem("JPEG (lossy, no transparency)", QByteArray("jpg"));
    m_format->addItem("WebP (lossy)", QByteArray("webp"));
    const auto supported = QImageWriter::supportedImageFormats();
    auto* model = qobject_cast<QStandardItemModel*>(m_format->model());
    for (int i = 0; i < m_format->count(); ++i) {
        if (!supported.contains(m_format->itemData(i).toByteArray())) {
            m_format->setItemText(i, m_format->itemText(i) + " — plugin unavailable");
            model->item(i)->setEnabled(false);
        }
    }
    form->addRow("Format", m_format);
    m_quality = new QSlider(Qt::Horizontal, this);
    m_quality->setObjectName("exportQuality");
    m_quality->setRange(1, 100);
    m_quality->setValue(90);
    auto* qualityLabel = new QLabel("90", this);
    auto* qualityRow = new QHBoxLayout;
    qualityRow->addWidget(m_quality);
    qualityRow->addWidget(qualityLabel);
    form->addRow("Quality", qualityRow);
    connect(m_quality, &QSlider::valueChanged, this, [qualityLabel](int v) { qualityLabel->setNum(v); });

    m_width = new QSpinBox(this);
    m_width->setObjectName("exportWidth");
    m_height = new QSpinBox(this);
    m_height->setObjectName("exportHeight");
    for (auto* spin : {m_width, m_height}) { spin->setRange(1, ImageExport::MaxDimension); spin->setSuffix(" px"); }
    m_width->setValue(original.width());
    m_height->setValue(original.height());
    form->addRow("Width", m_width);
    form->addRow("Height", m_height);
    m_lockRatio = new QCheckBox("Keep original aspect ratio", this);
    m_lockRatio->setObjectName("exportKeepRatio");
    m_lockRatio->setChecked(true);
    form->addRow(m_lockRatio);
    m_percent = new QDoubleSpinBox(this);
    m_percent->setObjectName("exportPercent");
    m_percent->setRange(0.01, 10000);
    m_percent->setDecimals(2);
    m_percent->setValue(100);
    m_percent->setSuffix(" %");
    form->addRow("Scale original", m_percent);
    m_interpolation = new QComboBox(this);
    m_interpolation->setObjectName("exportInterpolation");
    m_interpolation->addItem("Nearest (pixel art)", int(ImageExport::Interpolation::Nearest));
    m_interpolation->addItem("Bilinear", int(ImageExport::Interpolation::Bilinear));
    m_interpolation->addItem("Bicubic", int(ImageExport::Interpolation::Bicubic));
    m_interpolation->addItem("Lanczos", int(ImageExport::Interpolation::Lanczos));
    m_interpolation->addItem("Area (downscaling)", int(ImageExport::Interpolation::Area));
    m_interpolation->setCurrentIndex(3);
    form->addRow("Interpolation", m_interpolation);
    m_transparent = new QCheckBox("Preserve transparency", this);
    m_transparent->setObjectName("exportTransparent");
    m_transparent->setChecked(true);
    form->addRow(m_transparent);
    m_backgroundButton = new QPushButton("#ffffff", this);
    form->addRow("Background", m_backgroundButton);
    connect(m_backgroundButton, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(m_background, this, "Export Background");
        if (color.isValid()) {
            m_background = color;
            m_backgroundButton->setText(color.name());
        }
    });
    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    form->addRow(m_hint);
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(m_buttons);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &ExportDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_width, qOverload<int>(&QSpinBox::valueChanged), this, [this] { dimensionsChanged(true); });
    connect(m_height, qOverload<int>(&QSpinBox::valueChanged), this, [this] { dimensionsChanged(false); });
    connect(m_lockRatio, &QCheckBox::toggled, this, [this] { dimensionsChanged(true); });
    connect(m_percent, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double percent) {
        if (m_updating) return;
        // Percentage controls uniform scaling from the original, not from the last edit.
        const double factor = std::min({percent / 100.0,
            double(ImageExport::MaxDimension) / m_original.width(),
            double(ImageExport::MaxDimension) / m_original.height()});
        m_updating = true;
        m_width->setValue(qRound(m_original.width() * factor));
        m_height->setValue(qRound(m_original.height() * factor));
        m_percent->setValue(100.0 * m_width->value() / m_original.width());
        m_updating = false;
        updateOptions();
    });
    connect(m_format, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { updateOptions(); });
    connect(m_transparent, &QCheckBox::toggled, this, [this] { updateOptions(); });
    updateOptions();
}

void ExportDialog::dimensionsChanged(bool widthChanged) {
    if (m_updating) return;
    m_updating = true;
    if (m_lockRatio->isChecked()) {
        const double factor = std::min({widthChanged ? double(m_width->value()) / m_original.width()
            : double(m_height->value()) / m_original.height(),
            double(ImageExport::MaxDimension) / m_original.width(),
            double(ImageExport::MaxDimension) / m_original.height()});
        m_width->setValue(qRound(m_original.width() * factor));
        m_height->setValue(qRound(m_original.height() * factor));
    }
    m_percent->setValue(100.0 * m_width->value() / m_original.width());
    m_updating = false;
    updateOptions();
}

void ExportDialog::updateOptions() {
    m_quality->setEnabled(format() != "png");
    m_transparent->setEnabled(format() != "jpg");
    m_backgroundButton->setEnabled(!transparent());
    m_percent->setEnabled(m_lockRatio->isChecked());
    const bool valid = ImageExport::validSize(outputSize()) && QImageWriter::supportedImageFormats().contains(format());
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(valid);
    m_hint->setText(valid ? QString("Output: %1 × %2 px. %3\nOnly the exported file is resized; the project is unchanged.")
        .arg(outputSize().width()).arg(outputSize().height())
        .arg(transparent() ? "Transparent background." : "Transparency is composited onto the background color.")
        : "Cannot export: choose an available format and a size up to 20,000 px per side / 100 megapixels.");
}

QSize ExportDialog::outputSize() const { return {m_width->value(), m_height->value()}; }
QByteArray ExportDialog::format() const { return m_format->currentData().toByteArray(); }
int ExportDialog::quality() const { return format() == "png" ? -1 : m_quality->value(); }
bool ExportDialog::transparent() const { return format() != "jpg" && m_transparent->isChecked(); }
ImageExport::Interpolation ExportDialog::interpolation() const {
    return static_cast<ImageExport::Interpolation>(m_interpolation->currentData().toInt());
}
void ExportDialog::accept() {
    if (ImageExport::validSize(outputSize()) && QImageWriter::supportedImageFormats().contains(format())) QDialog::accept();
}
