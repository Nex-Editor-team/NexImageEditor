#include "LayersPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QVariant>

LayersPanel::LayersPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    layout->addWidget(new QLabel("<b>Layers</b>"));

    m_list = new QListWidget(this);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* item = pos == QPoint(-1, -1) ? m_list->currentItem() : m_list->itemAt(pos);
        emit layerContextMenuRequested(layerForItem(item), m_list->viewport()->mapToGlobal(pos));
    });
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    auto* addRow = new QHBoxLayout();
    auto* addRaster = new QToolButton(this); addRaster->setText("+Img");
    auto* addText = new QToolButton(this);   addText->setText("+Text");
    auto* addShape = new QToolButton(this);  addShape->setText("+Shape");
    addRow->addWidget(addRaster);
    addRow->addWidget(addText);
    addRow->addWidget(addShape);
    layout->addLayout(addRow);

    auto* editRow = new QHBoxLayout();
    auto* up = new QToolButton(this);     up->setText("▲");
    auto* down = new QToolButton(this);   down->setText("▼");
    auto* dup = new QToolButton(this);    dup->setText("Duplicate");
    auto* del = new QToolButton(this);    del->setText("Delete");
    editRow->addWidget(up);
    editRow->addWidget(down);
    editRow->addWidget(dup);
    editRow->addWidget(del);
    layout->addLayout(editRow);

    // Copy/Paste "trong" app (khác Duplicate: Copy giữ lại trong clipboard
    // nội bộ nên có thể Paste nhiều lần, kể cả sau khi đổi layer đang chọn).
    auto* copyPasteRow = new QHBoxLayout();
    auto* copyBtn = new QToolButton(this);  copyBtn->setText("Copy");
    auto* pasteBtn = new QToolButton(this); pasteBtn->setText("Paste");
    copyPasteRow->addWidget(copyBtn);
    copyPasteRow->addWidget(pasteBtn);
    layout->addLayout(copyPasteRow);

    connect(addRaster, &QToolButton::clicked, this, &LayersPanel::addRasterRequested);
    connect(addText, &QToolButton::clicked, this, &LayersPanel::addTextRequested);
    connect(addShape, &QToolButton::clicked, this, &LayersPanel::addShapeRequested);
    connect(up, &QToolButton::clicked, this, [this] {
        if (Layer* l = layerForItem(m_list->currentItem())) emit moveUpRequested(l);
    });
    connect(down, &QToolButton::clicked, this, [this] {
        if (Layer* l = layerForItem(m_list->currentItem())) emit moveDownRequested(l);
    });
    connect(dup, &QToolButton::clicked, this, [this] {
        if (Layer* l = layerForItem(m_list->currentItem())) emit duplicateRequested(l);
    });
    connect(del, &QToolButton::clicked, this, [this] {
        if (Layer* l = layerForItem(m_list->currentItem())) emit deleteRequested(l);
    });
    connect(copyBtn, &QToolButton::clicked, this, [this] {
        if (Layer* l = layerForItem(m_list->currentItem())) emit copyRequested(l);
    });
    connect(pasteBtn, &QToolButton::clicked, this, &LayersPanel::pasteRequested);
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* cur, QListWidgetItem*) {
        emit layerSelected(layerForItem(cur));
    });
    connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (Layer* l = layerForItem(item))
            emit visibilityToggled(l, item->checkState() == Qt::Checked);
    });
}

void LayersPanel::setProject(Project* project) {
    m_project = project;
    refresh();
}

Layer* LayersPanel::layerForItem(QListWidgetItem* item) const {
    if (!item) return nullptr;
    return reinterpret_cast<Layer*>(item->data(Qt::UserRole).value<quintptr>());
}

void LayersPanel::refresh() {
    m_list->blockSignals(true);
    m_list->clear();
    if (m_project) {
        // Top-most layer first, matching the visual stacking order.
        const auto& layers = m_project->layers();
        for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
            Layer* l = it->get();
            auto* item = new QListWidgetItem(l->name);
            item->setData(Qt::UserRole, QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(l)));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(l->visible ? Qt::Checked : Qt::Unchecked);
            m_list->addItem(item);
        }
    }
    m_list->blockSignals(false);
}

void LayersPanel::selectLayer(Layer* layer) {
    if (!layer) {
        m_list->setCurrentRow(-1);
        m_list->clearSelection();
        return;
    }
    for (int i = 0; i < m_list->count(); ++i) {
        if (layerForItem(m_list->item(i)) == layer) {
            m_list->setCurrentRow(i);
            return;
        }
    }
}
