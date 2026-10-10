#include "ui/Dialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <algorithm>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

NewDocumentDialog::NewDocumentDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("New");
    m_w = new QSpinBox(this);
    m_h = new QSpinBox(this);
    for (QSpinBox* s : {m_w, m_h}) s->setRange(1, 20000);
    m_w->setValue(1920);
    m_h->setValue(1080);
    m_bg = new QComboBox(this);
    m_bg->addItems({"White", "Transparent", "Black"});
    auto* form = new QFormLayout;
    form->addRow("Width (px):", m_w);
    form->addRow("Height (px):", m_h);
    form->addRow("Background:", m_bg);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* v = new QVBoxLayout(this);
    v->addLayout(form);
    v->addWidget(box);
}

int NewDocumentDialog::width() const { return m_w->value(); }
int NewDocumentDialog::height() const { return m_h->value(); }
QColor NewDocumentDialog::background() const {
    switch (m_bg->currentIndex()) {
    case 1: return QColor(0, 0, 0, 0);
    case 2: return Qt::black;
    default: return Qt::white;
    }
}

SizeDialog::SizeDialog(QWidget* parent, const QString& title, int w, int h, bool canvasMode)
    : QDialog(parent), m_ratio(w > 0 ? double(h) / w : 1.0) {
    setWindowTitle(title);
    m_w = new QSpinBox(this);
    m_h = new QSpinBox(this);
    for (QSpinBox* s : {m_w, m_h}) s->setRange(1, 20000);
    m_w->setValue(w);
    m_h->setValue(h);
    auto* form = new QFormLayout;
    form->addRow("Width (px):", m_w);
    form->addRow("Height (px):", m_h);
    if (canvasMode) {
        m_keep = nullptr;
        m_anchor = new QComboBox(this);
        m_anchor->addItems({"Top left", "Top", "Top right", "Left", "Centre", "Right",
                            "Bottom left", "Bottom", "Bottom right"});
        m_anchor->setCurrentIndex(4);
        form->addRow("Anchor:", m_anchor);
    } else {
        m_keep = new QCheckBox("Keep aspect ratio", this);
        m_keep->setChecked(true);
        form->addRow(m_keep);
        connect(m_w, &QSpinBox::valueChanged, this, [this](int v) {
            if (m_updating || !m_keep->isChecked()) return;
            m_updating = true;
            m_h->setValue(int(v * m_ratio + 0.5));
            m_updating = false;
        });
        connect(m_h, &QSpinBox::valueChanged, this, [this](int v) {
            if (m_updating || !m_keep->isChecked()) return;
            m_updating = true;
            m_w->setValue(int(v / m_ratio + 0.5));
            m_updating = false;
        });
    }
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* v = new QVBoxLayout(this);
    v->addLayout(form);
    v->addWidget(box);
}

int SizeDialog::width() const { return m_w->value(); }
int SizeDialog::height() const { return m_h->value(); }
int SizeDialog::anchorX() const { return m_anchor ? m_anchor->currentIndex() % 3 : 1; }
int SizeDialog::anchorY() const { return m_anchor ? m_anchor->currentIndex() / 3 : 1; }

FilterDialog::FilterDialog(QWidget* parent, const FilterSpec& spec, const QImage& source)
    : QDialog(parent), m_spec(spec) {
    setWindowTitle(spec.title);
    // Preview works on a copy no larger than ~420px on its longest side.
    QImage small = source;
    const int maxSide = 420;
    if (std::max(source.width(), source.height()) > maxSide)
        small = source.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_src = toPixels(small);

    auto* grid = new QGridLayout;
    auto* form = new QFormLayout;
    for (const FilterParam& fp : spec.params) {
        auto* row = new QHBoxLayout;
        auto* s = new QSlider(Qt::Horizontal, this);
        s->setRange(fp.min, fp.max);
        s->setValue(fp.def);
        auto* lbl = new QLabel(QString::number(fp.def), this);
        lbl->setMinimumWidth(40);
        row->addWidget(s, 1);
        row->addWidget(lbl);
        form->addRow(fp.label, row);
        connect(s, &QSlider::valueChanged, this, [this, lbl](int v) {
            lbl->setText(QString::number(v));
            updatePreview();
        });
        m_sliders.push_back(s);
    }
    m_preview = new QLabel(this);
    m_preview->setMinimumSize(small.width(), small.height());
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setStyleSheet("background:#222;");

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* v = new QVBoxLayout(this);
    v->addWidget(m_preview, 1);
    v->addLayout(form);
    v->addWidget(box);
    updatePreview();
    Q_UNUSED(grid)
}

std::vector<int> FilterDialog::values() const {
    std::vector<int> out;
    for (QSlider* s : m_sliders) out.push_back(s->value());
    return out;
}

void FilterDialog::updatePreview() {
    Pixels p = m_src;
    m_spec.apply(p, values());
    QImage img = fromPixels(p);
    m_preview->setPixmap(QPixmap::fromImage(img));
}
