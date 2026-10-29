#include "LayersPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QVariant>
#include <QSignalBlocker>
#include <QIcon>
#include <QPixmap>

LayersPanel::LayersPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* heading = new QLabel("    LAYERS", this);
    heading->setMinimumHeight(38);
    heading->setStyleSheet("font-weight: 600; border-bottom: 1px solid #414245;");
    layout->addWidget(heading);
    m_list = new QListWidget(this);
    m_list->setIconSize(QSize(36, 36));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    auto* row = new QHBoxLayout();
    row->setContentsMargins(8, 5, 8, 5);
    row->setSpacing(2);
    auto makeButton = [this, row](const QString& text, const QString& tip) {
        auto* button = new QToolButton(this);
        button->setText(text); button->setToolTip(tip);
        row->addWidget(button);
        return button;
    };
    auto* addRaster = makeButton("▧", "Add image layer");
    auto* addText = makeButton("T", "Add text layer");
    auto* addShape = makeButton("▱", "Add shape layer");
    row->addStretch();
    auto* up = makeButton("↑", "Move layer up");
    auto* down = makeButton("↓", "Move layer down");
    auto* dup = makeButton("▣", "Duplicate layer");
    auto* del = makeButton("×", "Delete layer");
    auto* copyBtn = makeButton("⎘", "Copy layer");
    auto* pasteBtn = makeButton("▤", "Paste layer");
    layout->addLayout(row);

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
            QString type = l->type() == LayerType::Raster ? "Image" : l->type() == LayerType::Text ? "Text" : "Shape";
            auto* item = new QListWidgetItem(l->name + "\n" + type);
            item->setSizeHint(QSize(0, 50));
            if (!l->displayContent().isNull()) {
                QPixmap thumbnail = QPixmap::fromImage(l->displayContent().scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                item->setIcon(QIcon(thumbnail));
            }
            item->setData(Qt::UserRole, QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(l)));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(l->visible ? Qt::Checked : Qt::Unchecked);
            m_list->addItem(item);
        }
    }
    m_list->blockSignals(false);
}

void LayersPanel::selectLayer(Layer* layer) {
    QSignalBlocker blocker(m_list);
    if (!layer) { m_list->setCurrentItem(nullptr); return; }
    for (int i = 0; i < m_list->count(); ++i) {
        if (layerForItem(m_list->item(i)) == layer) {
            m_list->setCurrentRow(i);
            return;
        }
    }
}
