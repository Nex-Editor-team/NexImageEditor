#include "ui/Panels.h"

#include "core/Layer.h"
#include "ui/CanvasView.h"
#include "ui/Icons.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QUndoView>
#include <QVBoxLayout>
#include <cmath>

// ---------------------------------------------------------------- Layers

LayersPanel::LayersPanel(QWidget* parent) : QWidget(parent) {
    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setIconSize(QSize(16, 16));
    connect(m_list, &QListWidget::currentRowChanged, this, &LayersPanel::onRowChanged);
    connect(m_list, &QListWidget::itemChanged, this, &LayersPanel::onItemChanged);

    auto* buttons = new QHBoxLayout;
    const struct { const char* text; const char* tip; } defs[] = {
        {"+", "New layer"}, {"Dup", "Duplicate layer"}, {"Del", "Delete layer"},
        {"Merge", "Merge down"}, {"Up", "Move up"}, {"Down", "Move down"}, {"Flat", "Flatten image"}};
    QList<QPushButton*> btns;
    for (const auto& d : defs) {
        auto* b = new QPushButton(d.text, this);
        b->setToolTip(d.tip);
        b->setFlat(true);
        buttons->addWidget(b);
        btns << b;
    }
    buttons->addStretch();
    connect(btns[0], &QPushButton::clicked, this, [this] { if (m_view) m_view->addLayer(); });
    connect(btns[1], &QPushButton::clicked, this, [this] { if (m_view) m_view->duplicateLayer(); });
    connect(btns[2], &QPushButton::clicked, this, [this] { if (m_view) m_view->deleteLayer(); });
    connect(btns[3], &QPushButton::clicked, this, [this] { if (m_view) m_view->mergeDownLayer(); });
    connect(btns[4], &QPushButton::clicked, this, [this] {
        if (!m_view) return;
        int i = m_view->document().activeIndex();
        if (i + 1 < int(m_view->document().layers().size())) m_view->moveLayer(i, i + 1);
    });
    connect(btns[5], &QPushButton::clicked, this, [this] {
        if (!m_view) return;
        int i = m_view->document().activeIndex();
        if (i > 0) m_view->moveLayer(i, i - 1);
    });
    connect(btns[6], &QPushButton::clicked, this, [this] { if (m_view) m_view->flattenImage(); });

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(4, 4, 4, 4);
    v->addWidget(m_list, 1);
    v->addLayout(buttons);
}

void LayersPanel::setView(CanvasView* view) {
    m_view = view;
    refresh();
}

int LayersPanel::layerIndexForRow(int row) const {
    return m_view ? int(m_view->document().layers().size()) - 1 - row : -1;
}

void LayersPanel::refresh() {
    m_updating = true;
    m_list->clear();
    if (m_view) {
        const Document& d = m_view->document();
        const int n = int(d.layers().size());
        for (int row = 0; row < n; ++row) {
            const Layer& l = d.layers()[size_t(n - 1 - row)];
            auto* item = new QListWidgetItem(Icons::visibility(l.visible), l.name);
            item->setData(Qt::UserRole, n - 1 - row);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
            item->setCheckState(l.visible ? Qt::Checked : Qt::Unchecked);
            m_list->addItem(item);
        }
        m_list->setCurrentRow(n - 1 - d.activeIndex());
    }
    m_updating = false;
}

void LayersPanel::onRowChanged(int row) {
    if (m_updating || !m_view || row < 0) return;
    m_view->selectLayer(layerIndexForRow(row));
}

void LayersPanel::onItemChanged(QListWidgetItem* item) {
    if (m_updating || !m_view) return;
    const int idx = item->data(Qt::UserRole).toInt();
    const bool vis = item->checkState() == Qt::Checked;
    const QString name = item->text();
    const Layer& cur = m_view->document().layers()[size_t(idx)];
    if (cur.visible != vis)
        m_view->editLayer(idx, "Layer Visibility", [vis](Layer& l) { l.visible = vis; });
    if (cur.name != name && !name.isEmpty())
        m_view->editLayer(idx, "Rename Layer", [name](Layer& l) { l.name = name; });
}

// ---------------------------------------------------------------- Properties

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent) {
    m_title = new QLabel(this);
    m_title->setStyleSheet("font-weight:bold;");
    m_name = new QLineEdit(this);
    m_opacity = new QSlider(Qt::Horizontal, this);
    m_opacity->setRange(0, 100);
    m_opacitySpin = new QSpinBox(this);
    m_opacitySpin->setRange(0, 100);
    m_opacitySpin->setSuffix(" %");
    m_blend = new QComboBox(this);
    m_blend->addItems(blendModeNames());
    m_locked = new QCheckBox("Lock layer", this);
    m_x = new QSpinBox(this);
    m_y = new QSpinBox(this);
    for (QSpinBox* s : {m_x, m_y}) s->setRange(-20000, 20000);
    m_size = new QLabel(this);

    auto* opRow = new QHBoxLayout;
    opRow->addWidget(m_opacity, 1);
    opRow->addWidget(m_opacitySpin);
    auto* pos = new QHBoxLayout;
    pos->addWidget(new QLabel("X", this));
    pos->addWidget(m_x);
    pos->addWidget(new QLabel("Y", this));
    pos->addWidget(m_y);

    auto* form = new QFormLayout;
    form->addRow("Name:", m_name);
    form->addRow("Opacity:", opRow);
    form->addRow("Blend:", m_blend);
    form->addRow("Position:", pos);
    form->addRow("Size:", m_size);
    form->addRow(m_locked);

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(6, 6, 6, 6);
    v->addWidget(m_title);
    v->addLayout(form);
    v->addStretch();

    auto applyOpacity = [this](int value) {
        if (m_updating || !m_view) return;
        m_opacitySpin->blockSignals(true);
        m_opacitySpin->setValue(value);
        m_opacitySpin->blockSignals(false);
        const int i = m_view->document().activeIndex();
        m_view->editLayer(i, "Layer Opacity", [value](Layer& l) { l.opacity = value / 100.0; });
    };
    connect(m_opacity, &QSlider::valueChanged, this, [this, applyOpacity](int v) {
        if (!m_opacity->isSliderDown()) applyOpacity(v);
    });
    connect(m_opacity, &QSlider::sliderReleased, this, [this, applyOpacity] { applyOpacity(m_opacity->value()); });
    connect(m_opacitySpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_updating || !m_view) return;
        m_opacity->blockSignals(true);
        m_opacity->setValue(v);
        m_opacity->blockSignals(false);
        m_view->editLayer(m_view->document().activeIndex(), "Layer Opacity", [v](Layer& l) { l.opacity = v / 100.0; });
    });
    connect(m_blend, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_updating || !m_view) return;
        m_view->editLayer(m_view->document().activeIndex(), "Blend Mode", [idx](Layer& l) { l.blend = BlendMode(idx); });
    });
    connect(m_locked, &QCheckBox::toggled, this, [this](bool on) {
        if (m_updating || !m_view) return;
        m_view->editLayer(m_view->document().activeIndex(), "Lock Layer", [on](Layer& l) { l.locked = on; });
    });
    connect(m_name, &QLineEdit::editingFinished, this, [this] {
        if (m_updating || !m_view) return;
        const QString n = m_name->text().trimmed();
        if (n.isEmpty()) return;
        m_view->editLayer(m_view->document().activeIndex(), "Rename Layer", [n](Layer& l) { l.name = n; });
    });
    auto applyPos = [this] {
        if (m_updating || !m_view) return;
        const int x = m_x->value(), y = m_y->value();
        m_view->editLayer(m_view->document().activeIndex(), "Move Layer", [x, y](Layer& l) { l.x = x; l.y = y; });
    };
    connect(m_x, qOverload<int>(&QSpinBox::valueChanged), this, [applyPos](int) { applyPos(); });
    connect(m_y, qOverload<int>(&QSpinBox::valueChanged), this, [applyPos](int) { applyPos(); });
}

void PropertiesPanel::setView(CanvasView* view) {
    m_view = view;
    refresh();
}

void PropertiesPanel::refresh() {
    m_updating = true;
    const Layer* l = m_view ? m_view->document().active() : nullptr;
    const bool on = l != nullptr;
    for (QWidget* w : {static_cast<QWidget*>(m_name), static_cast<QWidget*>(m_opacity),
                       static_cast<QWidget*>(m_opacitySpin), static_cast<QWidget*>(m_blend),
                       static_cast<QWidget*>(m_locked), static_cast<QWidget*>(m_x),
                       static_cast<QWidget*>(m_y)})
        w->setEnabled(on);
    if (l) {
        const int pct = int(std::lround(l->opacity * 100));
        m_title->setText(l->background ? "Background" : "Layer");
        m_name->setText(l->name);
        m_opacity->setValue(pct);
        m_opacitySpin->setValue(pct);
        m_blend->setCurrentIndex(int(l->blend));
        m_locked->setChecked(l->locked);
        m_x->setValue(l->x);
        m_y->setValue(l->y);
        m_size->setText(QString("%1 × %2 px").arg(l->w()).arg(l->h()));
    } else {
        m_title->setText("No layer");
        m_size->clear();
    }
    m_updating = false;
}

// ---------------------------------------------------------------- History

HistoryPanel::HistoryPanel(QWidget* parent) : QWidget(parent) {
    m_list = new QUndoView(this);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(4, 4, 4, 4);
    v->addWidget(m_list);
}

void HistoryPanel::setView(CanvasView* view) {
    m_list->setStack(view ? view->undoStack() : nullptr);
}
