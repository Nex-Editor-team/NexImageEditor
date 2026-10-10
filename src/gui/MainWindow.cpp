#include "MainWindow.h"
#include "../project/Commands.h"
#include "../project/TextLayer.h"
#include "../project/ShapeLayer.h"
#include "../engine/Transform.h"
#include "../io/ImageIO.h"
#include "../io/ProjectIO.h"

#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QDockWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QActionGroup>
#include <QLineEdit>
#include <QFileInfo>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSlider>
#include <QLabel>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QCloseEvent>
#include <QSpinBox>
#include <QCheckBox>
#include <QSignalBlocker>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_project = std::make_unique<Project>(QSize(1920, 1080));
    m_undoStack = new QUndoStack(this);

    m_canvas = new Canvas(this);
    m_canvas->setProject(m_project.get());
    m_canvas->setUndoStack(m_undoStack);
    setCentralWidget(m_canvas);

    m_layersPanel = new LayersPanel(this);
    auto* layersDock = new QDockWidget("Layers", this);
    layersDock->setWidget(m_layersPanel);
    addDockWidget(Qt::RightDockWidgetArea, layersDock);

    m_propertiesPanel = new PropertiesPanel(this);
    m_propertiesPanel->setUndoStack(m_undoStack);
    m_propertiesDock = new QDockWidget("Properties", this);
    m_propertiesDock->setWidget(m_propertiesPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    m_propertiesDock->setVisible(false); // hidden until a layer is selected

    // Photo Adjustments Panel (left side dock)
    m_photoPanel = new PhotoAdjustmentsPanel(this);
    m_photoPanel->setUndoStack(m_undoStack);
    m_photoDock = new QDockWidget("Chỉnh sửa ảnh", this);
    m_photoDock->setWidget(m_photoPanel);
    m_photoDock->setMinimumWidth(280);
    addDockWidget(Qt::LeftDockWidgetArea, m_photoDock);
    m_photoDock->setVisible(false); // hidden until a layer is selected

    syncPanelsFromProject();
    buildMenusAndToolbar();
    refreshTitle();
    connect(m_undoStack, &QUndoStack::cleanChanged, this, &MainWindow::refreshTitle);
    connect(m_undoStack, &QUndoStack::indexChanged, this, &MainWindow::refreshAfterCommand);

    // ---- wiring: canvas <-> panels <-> project ----

    connect(m_canvas, &Canvas::layerContextMenuRequested, this, &MainWindow::showLayerContextMenu);
    connect(m_layersPanel, &LayersPanel::layerContextMenuRequested, this, &MainWindow::showLayerContextMenu);
    connect(m_canvas, &Canvas::imageFilesDropped, this, &MainWindow::importImages);
    connect(m_canvas, &Canvas::selectionChanged, this, &MainWindow::onCanvasSelectionChanged);
    connect(m_canvas, &Canvas::transformLiveChanged, this, [this](Layer*) {
        m_propertiesPanel->refreshFromLayer();
    });

    connect(m_layersPanel, &LayersPanel::layerSelected, this, &MainWindow::onLayerSelectedInPanel);
    connect(m_layersPanel, &LayersPanel::addRasterRequested, this, &MainWindow::onAddRasterLayer);
    connect(m_layersPanel, &LayersPanel::addTextRequested, this, &MainWindow::onAddTextLayer);
    connect(m_layersPanel, &LayersPanel::addShapeRequested, this, &MainWindow::onAddShapeLayer);
    connect(m_layersPanel, &LayersPanel::deleteRequested, this, &MainWindow::onDeleteLayer);
    connect(m_layersPanel, &LayersPanel::duplicateRequested, this, &MainWindow::onDuplicateLayer);
    connect(m_layersPanel, &LayersPanel::copyRequested, this, &MainWindow::onCopyLayer);
    connect(m_layersPanel, &LayersPanel::pasteRequested, this, &MainWindow::onPasteLayer);
    connect(m_layersPanel, &LayersPanel::moveUpRequested, this, &MainWindow::onMoveLayerUp);
    connect(m_layersPanel, &LayersPanel::moveDownRequested, this, &MainWindow::onMoveLayerDown);
    connect(m_layersPanel, &LayersPanel::visibilityToggled, this, &MainWindow::onLayerVisibilityToggled);

    connect(m_propertiesPanel, &PropertiesPanel::requestCanvasUpdate, m_canvas, qOverload<>(&QWidget::update));
    connect(m_photoPanel, &PhotoAdjustmentsPanel::requestCanvasUpdate, m_canvas, qOverload<>(&QWidget::update));
}

void MainWindow::buildMenusAndToolbar() {
    // ---- File menu ----
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("New Project", this, &MainWindow::onNewProject, QKeySequence::New);
    fileMenu->addAction("Open Project...", this, &MainWindow::onOpenProject, QKeySequence::Open);
    fileMenu->addAction("Save Project", this, &MainWindow::onSaveProject, QKeySequence::Save);
    fileMenu->addAction("Save Project As...", this, &MainWindow::onSaveProjectAs, QKeySequence::SaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction("Import Image as Layer...", this, &MainWindow::onImportImage, QKeySequence("Ctrl+I"));
    fileMenu->addAction("Export Flattened Image...", this, &MainWindow::onExportImage, QKeySequence("Ctrl+E"));
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", this, &QWidget::close, QKeySequence::Quit);

    // ---- Edit menu ----
    QMenu* editMenu = menuBar()->addMenu("&Edit");
    QAction* undoAction = m_undoStack->createUndoAction(this, "Undo");
    undoAction->setShortcut(QKeySequence::Undo);
    QAction* redoAction = m_undoStack->createRedoAction(this, "Redo");
    redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);
    editMenu->addSeparator();

    // Copy/Paste "trong" app: nhân bản layer qua một clipboard nội bộ.
    QAction* copyAction = editMenu->addAction("Copy Layer", QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, [this] { onCopyLayer(m_canvas->selectedLayer()); });

    editMenu->addAction("Paste Layer", this, &MainWindow::onPasteLayer, QKeySequence::Paste);
    editMenu->addSeparator();

    // Copy/Paste "ngoài" app: qua clipboard của hệ điều hành (dạng ảnh),
    // để có thể dán sang ứng dụng khác hoặc dán ảnh chụp/copy từ nơi khác vào.
    editMenu->addAction("Copy Layer to System Clipboard", this,
                         &MainWindow::onCopyLayerToSystemClipboard, QKeySequence("Ctrl+Shift+C"));
    editMenu->addAction("Paste Image from System Clipboard", this,
                         &MainWindow::onPasteImageFromSystemClipboard, QKeySequence("Ctrl+Shift+V"));

    // ---- Layer menu ----
    QMenu* layerMenu = menuBar()->addMenu("&Layer");
    layerMenu->addAction("Add Image Layer...", this, &MainWindow::onAddRasterLayer);
    layerMenu->addAction("Add Text Layer", this, &MainWindow::onAddTextLayer);
    layerMenu->addAction("Add Shape Layer", this, &MainWindow::onAddShapeLayer);

    layerMenu->addAction("Duplicate Layer", this, [this] { onDuplicateLayer(m_canvas->selectedLayer()); }, QKeySequence("Ctrl+D"));
    layerMenu->addAction("Delete Layer", this, [this] { onDeleteLayer(m_canvas->selectedLayer()); }, QKeySequence::Delete);

    QMenu* canvasMenu = menuBar()->addMenu("&Canvas");
    canvasMenu->addAction("Canvas Size...", this, &MainWindow::onResizeCanvas, QKeySequence("Ctrl+Alt+C"));

    // ---- Transform/Image menu ----
    QMenu* imageMenu = menuBar()->addMenu("&Transform");
    imageMenu->addAction("Flip Horizontal", this, &MainWindow::onFlipHorizontal, QKeySequence("Shift+H"));
    imageMenu->addAction("Flip Vertical", this, &MainWindow::onFlipVertical, QKeySequence("Shift+V"));
    QAction* rotCW = imageMenu->addAction("Rotate 90° CW");
    rotCW->setShortcut(QKeySequence("Ctrl+]"));
    connect(rotCW, &QAction::triggered, this, [this] { onRotate90(true); });

    QAction* rotCCW = imageMenu->addAction("Rotate 90° CCW");
    rotCCW->setShortcut(QKeySequence("Ctrl+["));
    connect(rotCCW, &QAction::triggered, this, [this] { onRotate90(false); });

    imageMenu->addAction("Reset Transform", this, &MainWindow::onResetTransform, QKeySequence("Ctrl+Shift+R"));

    // ---- Toolbar: tools + zoom ----
    QToolBar* toolbar = addToolBar("Tools");
    toolbar->setMovable(false);

    auto* toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);

    QAction* selectTool = toolbar->addAction("Select / Transform");
    selectTool->setCheckable(true);
    selectTool->setChecked(true);
    toolGroup->addAction(selectTool);
    connect(selectTool, &QAction::toggled, this, [this](bool on) {
        if (on) m_canvas->setTool(ToolMode::Select);
    });

    QAction* panTool = toolbar->addAction("Pan");
    panTool->setCheckable(true);
    toolGroup->addAction(panTool);
    connect(panTool, &QAction::toggled, this, [this](bool on) {
        if (on) m_canvas->setTool(ToolMode::Pan);
    });

    toolbar->addSeparator();
    QAction* zoomFit = toolbar->addAction("Zoom to Fit");
    connect(zoomFit, &QAction::triggered, this, [this] { m_canvas->zoomToFit(); m_canvas->update(); });
    QAction* zoomIn = toolbar->addAction("Zoom In");
    connect(zoomIn, &QAction::triggered, this, [this] { m_canvas->setZoom(m_canvas->zoom() * 1.25); });
    QAction* zoomOut = toolbar->addAction("Zoom Out");
    connect(zoomOut, &QAction::triggered, this, [this] { m_canvas->setZoom(m_canvas->zoom() / 1.25); });

    toolbar->addSeparator();
    toolbar->addAction(undoAction);
    toolbar->addAction(redoAction);
}

void MainWindow::refreshTitle() {
    QString name = m_project->filePath.isEmpty() ? "Untitled" : QFileInfo(m_project->filePath).fileName();
    setWindowTitle(QString("%1[*] — NexImageEditor").arg(name));
    setWindowModified(!m_undoStack->isClean());
}

void MainWindow::syncPanelsFromProject() {
    m_layersPanel->setProject(m_project.get());
    m_propertiesPanel->setProject(m_project.get());
    m_photoPanel->setProject(m_project.get());
}

void MainWindow::updateSelection(Layer* layer) {
    m_propertiesPanel->setLayer(layer);
    m_propertiesDock->setVisible(layer != nullptr);
    // Only show Photo Adjustments panel for raster layers (where it's most useful)
    bool showPhoto = (layer != nullptr);
    m_photoPanel->setLayer(layer);
    m_photoDock->setVisible(showPhoto);
}

// ---------------- File actions ----------------

void MainWindow::onNewProject() {
    bool ok = false;
    int w = QInputDialog::getInt(this, "New Project", "Width:", 1920, 1, 20000, 1, &ok);
    if (!ok) return;
    int h = QInputDialog::getInt(this, "New Project", "Height:", 1080, 1, 20000, 1, &ok);
    if (!ok) return;

    if (!maybeSave()) return;
    m_canvas->setSelectedLayer(nullptr);
    updateSelection(nullptr);
    m_undoStack->clear();
    m_project = std::make_unique<Project>(QSize(w, h));
    m_canvas->setProject(m_project.get());
    syncPanelsFromProject();
    m_layersPanel->refresh();
    updateSelection(nullptr);
    refreshTitle();
}

void MainWindow::onOpenProject() {
    QString path = QFileDialog::getOpenFileName(this, "Open Project", QString(), "NexImageEditor Project (*.nxproj)");
    if (path.isEmpty() || !maybeSave()) return;

    auto loaded = std::make_unique<Project>();
    if (!ProjectIO::loadProject(path, *loaded)) {
        QMessageBox::warning(this, "Open Project", "Could not read this project file.");
        return;
    }
    m_canvas->setSelectedLayer(nullptr);
    updateSelection(nullptr);
    m_undoStack->clear();
    m_project = std::move(loaded);
    m_canvas->setProject(m_project.get());
    syncPanelsFromProject();
    m_layersPanel->refresh();
    updateSelection(nullptr);
    refreshTitle();
}

void MainWindow::onSaveProject() {
    if (m_project->filePath.isEmpty()) { onSaveProjectAs(); return; }
    if (!ProjectIO::saveProject(m_project->filePath, *m_project))
        QMessageBox::warning(this, "Save Project", "Could not save the project.");
    else
        m_undoStack->setClean();
}

void MainWindow::onSaveProjectAs() {
    QString path = QFileDialog::getSaveFileName(this, "Save Project As", "untitled.nxproj",
                                                 "NexImageEditor Project (*.nxproj)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".nxproj")) path += ".nxproj";

    if (!ProjectIO::saveProject(path, *m_project)) {
        QMessageBox::warning(this, "Save Project", "Could not save the project.");
        return;
    }
    m_project->filePath = path;
    m_undoStack->setClean();
    refreshTitle();
}

void MainWindow::onImportImage() {
    QString path = QFileDialog::getOpenFileName(this, "Import Image", QString(),
                                                 "Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff)");
    if (path.isEmpty()) return;

    importImages({path}, QPointF(m_project->canvasSize().width() / 2.0,
                                m_project->canvasSize().height() / 2.0));
}

void MainWindow::importImages(const QStringList& paths, QPointF position) {
    QStringList failures;
    Layer* lastImported = nullptr;
    bool macroStarted = false;
    for (const QString& path : paths) {
        ImageBuffer buf = ImageIO::loadImage(path);
        if (buf.isNull()) {
            failures.append(QFileInfo(path).fileName());
            continue;
        }
        // Do not create an empty undo entry when every file fails to load.
        if (!macroStarted) {
            m_undoStack->beginMacro(paths.size() > 1 ? "Import Images" : "Import Image");
            macroStarted = true;
        }
        auto layer = std::make_unique<Layer>(LayerType::Raster, QFileInfo(path).baseName());
        layer->content = buf.image();
        layer->transform.center = position;
        lastImported = layer.get();
        m_undoStack->push(new AddLayerCommand(m_project.get(), std::move(layer)));
    }
    if (macroStarted) {
        m_undoStack->endMacro();
        m_canvas->setSelectedLayer(lastImported);
        m_canvas->setFocus();
    }
    if (!failures.isEmpty()) {
        auto* warning = new QMessageBox(QMessageBox::Warning, "Import Images",
            "Could not read these image files:\n" + failures.join("\n"), QMessageBox::Ok, this);
        warning->setTextFormat(Qt::PlainText);
        warning->setAttribute(Qt::WA_DeleteOnClose);
        warning->open();
    }
}

void MainWindow::onExportImage() {
    QDialog dialog(this);
    dialog.setWindowTitle("Export Image");
    auto* form = new QFormLayout(&dialog);

    auto* formatCombo = new QComboBox(&dialog);
    formatCombo->addItem("PNG (lossless)");
    formatCombo->addItem("JPEG (lossy)");
    formatCombo->addItem("WebP (lossy)");
    form->addRow("Format", formatCombo);

    auto* qualityRow = new QHBoxLayout();
    auto* qualitySlider = new QSlider(Qt::Horizontal, &dialog);
    qualitySlider->setRange(1, 100);
    qualitySlider->setValue(90);
    auto* qualityLabel = new QLabel("90", &dialog);
    qualityRow->addWidget(qualitySlider);
    qualityRow->addWidget(qualityLabel);
    form->addRow("Quality", qualityRow);
    connect(qualitySlider, &QSlider::valueChanged, &dialog, [qualityLabel](int v) {
        qualityLabel->setText(QString::number(v));
    });

    auto updateQualityEnabled = [formatCombo, qualitySlider] {
        qualitySlider->setEnabled(formatCombo->currentIndex() != 0); // PNG is lossless, no quality knob
    };
    connect(formatCombo, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, [updateQualityEnabled](int) {
        updateQualityEnabled();
    });
    updateQualityEnabled();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return;

    int formatIndex = formatCombo->currentIndex();
    QString ext = formatIndex == 0 ? "png" : formatIndex == 1 ? "jpg" : "webp";
    QString filter = formatIndex == 0 ? "PNG (*.png)" : formatIndex == 1 ? "JPEG (*.jpg *.jpeg)" : "WebP (*.webp)";

    QString path = QFileDialog::getSaveFileName(this, "Export Image", "export." + ext, filter);
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += "." + ext;

    int quality = (formatIndex == 0) ? -1 : qualitySlider->value();

    ImageBuffer flat = m_project->flatten();
    if (!ImageIO::saveImage(path, flat, quality))
        QMessageBox::warning(this, "Export Image", "Could not write this image file.");
}

// ---------------- Layer actions ----------------

void MainWindow::onAddRasterLayer() { onImportImage(); }

void MainWindow::onAddTextLayer() {
    bool ok = false;
    QString text = QInputDialog::getText(this, "Add Text Layer", "Text:", QLineEdit::Normal, "Text", &ok);
    if (!ok || text.isEmpty()) return;

    auto layer = std::make_unique<TextLayer>(text);
    layer->transform.center = QPointF(m_project->canvasSize().width() / 2.0,
                                       m_project->canvasSize().height() / 2.0);
    auto* cmd = new AddLayerCommand(m_project.get(), std::move(layer));
    m_undoStack->push(cmd);
}

void MainWindow::onAddShapeLayer() {
    auto layer = std::make_unique<ShapeLayer>(ShapeKind::Rectangle);
    layer->transform.center = QPointF(m_project->canvasSize().width() / 2.0,
                                       m_project->canvasSize().height() / 2.0);
    auto* cmd = new AddLayerCommand(m_project.get(), std::move(layer));
    m_undoStack->push(cmd);
}

void MainWindow::onDeleteLayer(Layer* layer) {
    if (!layer) return;
    int idx = m_project->indexOf(layer);
    if (idx < 0) return;
    if (m_canvas->selectedLayer() == layer) m_canvas->setSelectedLayer(nullptr);
    m_undoStack->push(new RemoveLayerCommand(m_project.get(), idx));
    updateSelection(nullptr);
}

void MainWindow::onDuplicateLayer(Layer* layer) {
    if (!layer) return;
    auto copy = layer->clone();
    copy->name += " copy";
    copy->transform.center += QPointF(20, 20);
    auto* cmd = new AddLayerCommand(m_project.get(), std::move(copy));
    m_undoStack->push(cmd);
}

void MainWindow::onCopyLayer(Layer* layer) {
    // "Copy trong": chỉ giữ một bản sao layer trong bộ nhớ của app (chưa
    // tạo layer mới ngay). Nhấn Paste (Ctrl+V) bao nhiêu lần cũng được,
    // mỗi lần tạo một layer mới độc lập từ bản đã copy.
    if (!layer) return;
    m_layerClipboard = layer->clone();
}

void MainWindow::onPasteLayer() {
    if (!m_layerClipboard) return;
    auto copy = m_layerClipboard->clone();
    copy->name += " copy";
    copy->transform.center += QPointF(20, 20);
    Layer* raw = copy.get();

    m_undoStack->push(new AddLayerCommand(m_project.get(), std::move(copy)));
    m_canvas->setSelectedLayer(raw);
    m_layersPanel->selectLayer(raw);
    updateSelection(raw);
}

void MainWindow::onCopyLayerToSystemClipboard() {
    // "Copy ngoài": đẩy ảnh của layer đang chọn ra clipboard hệ thống,
    // để dán được sang ứng dụng khác (trình soạn thảo, chat, ảnh, v.v.).
    Layer* l = m_canvas->selectedLayer();
    if (!l) return;
    QImage img = l->displayContent();
    if (img.isNull()) {
        QMessageBox::information(this, "Copy Layer", "Layer này không có dữ liệu ảnh để copy.");
        return;
    }
    QApplication::clipboard()->setImage(img, QClipboard::Clipboard);
}

void MainWindow::onPasteImageFromSystemClipboard() {
    // "Paste ngoài": lấy ảnh từ clipboard hệ thống (copy từ trình duyệt,
    // ảnh chụp màn hình, ứng dụng khác...) và tạo thành một layer raster mới.
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (!mime || !mime->hasImage()) {
        QMessageBox::information(this, "Paste Image", "Clipboard hệ thống hiện không chứa ảnh.");
        return;
    }
    QImage img = qvariant_cast<QImage>(mime->imageData());
    if (img.isNull()) return;

    auto layer = std::make_unique<Layer>(LayerType::Raster, "Pasted Image");
    layer->content = img.convertToFormat(QImage::Format_ARGB32);
    layer->transform.center = QPointF(m_project->canvasSize().width() / 2.0,
                                       m_project->canvasSize().height() / 2.0);
    Layer* raw = layer.get();

    m_undoStack->push(new AddLayerCommand(m_project.get(), std::move(layer)));
    m_canvas->setSelectedLayer(raw);
    m_layersPanel->selectLayer(raw);
    updateSelection(raw);
}

void MainWindow::onMoveLayerUp(Layer* layer) {
    if (!layer) return;
    int idx = m_project->indexOf(layer);
    if (idx < 0 || idx == m_project->layerCount() - 1) return;
    m_undoStack->push(new MoveLayerCommand(m_project.get(), idx, idx + 1));
    m_layersPanel->selectLayer(layer);
}

void MainWindow::onMoveLayerDown(Layer* layer) {
    if (!layer) return;
    int idx = m_project->indexOf(layer);
    if (idx <= 0) return;
    m_undoStack->push(new MoveLayerCommand(m_project.get(), idx, idx - 1));
    m_layersPanel->selectLayer(layer);
}

void MainWindow::onLayerVisibilityToggled(Layer* layer, bool visible) {
    if (!layer) return;
    if (layer->visible == visible) return;
    m_undoStack->push(new SetLayerPropertyCommand<bool>(m_project.get(), layer->id(),
        layer->visible, visible, [](Layer* l) { return l->visible; },
        [](Layer* l, bool v) { l->visible = v; }, "Change Layer Visibility"));
}

void MainWindow::onLayerSelectedInPanel(Layer* layer) {
    m_canvas->setSelectedLayer(layer);
    updateSelection(layer);
}

void MainWindow::onCanvasSelectionChanged(Layer* layer) {
    m_layersPanel->selectLayer(layer);
    updateSelection(layer);
}

// ---------------- Transform actions ----------------

void MainWindow::onFlipHorizontal() {
    Layer* l = m_canvas->selectedLayer();
    if (!l) return;
    auto before = l->transform;
    auto after = before;
    after.scaleX = -after.scaleX;
    l->transform = after;
    m_undoStack->push(new TransformLayerCommand(m_project.get(), l->id(), before, after));
    m_propertiesPanel->refreshFromLayer();
    m_canvas->update();
}

void MainWindow::onFlipVertical() {
    Layer* l = m_canvas->selectedLayer();
    if (!l) return;
    auto before = l->transform;
    auto after = before;
    after.scaleY = -after.scaleY;
    l->transform = after;
    m_undoStack->push(new TransformLayerCommand(m_project.get(), l->id(), before, after));
    m_propertiesPanel->refreshFromLayer();
    m_canvas->update();
}

void MainWindow::onRotate90(bool clockwise) {
    Layer* l = m_canvas->selectedLayer();
    if (!l) return;
    auto before = l->transform;
    auto after = before;
    after.rotationDeg += clockwise ? 90.0 : -90.0;
    l->transform = after;
    m_undoStack->push(new TransformLayerCommand(m_project.get(), l->id(), before, after));
    m_propertiesPanel->refreshFromLayer();
    m_canvas->update();
}

void MainWindow::onResetTransform() {
    Layer* l = m_canvas->selectedLayer();
    if (!l) return;
    auto before = l->transform;
    Transform::FreeTransformState after; // defaults: scale 1, rotation 0
    after.center = QPointF(m_project->canvasSize().width() / 2.0, m_project->canvasSize().height() / 2.0);
    l->transform = after;
    m_undoStack->push(new TransformLayerCommand(m_project.get(), l->id(), before, after));
    m_propertiesPanel->refreshFromLayer();
    m_canvas->update();
}


bool MainWindow::maybeSave() {
    if (m_undoStack->isClean()) return true;
    const auto answer = QMessageBox::warning(this, "Unsaved Changes",
        "Save changes to this project before continuing?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Discard) return true;
    onSaveProject();
    return m_undoStack->isClean(); // A cancelled or failed save must not discard work.
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) event->accept();
    else event->ignore();
}

void MainWindow::refreshAfterCommand() {
    Layer* selected = m_canvas->selectedLayer();
    if (m_project->indexOf(selected) < 0) {
        selected = nullptr;
        m_canvas->setSelectedLayer(nullptr);
        updateSelection(nullptr);
    }
    // Restoring the same selection is UI bookkeeping, not a user selection.
    const QSignalBlocker blockSelection(m_layersPanel);
    m_layersPanel->refresh();
    m_layersPanel->selectLayer(selected);
    m_propertiesPanel->refreshFromLayer();
    m_photoPanel->refreshFromLayer();
    m_canvas->update();
}

void MainWindow::onResizeCanvas() {
    QDialog dialog(this);
    dialog.setWindowTitle("Canvas Size");
    auto* form = new QFormLayout(&dialog);
    auto* width = new QSpinBox(&dialog);
    auto* height = new QSpinBox(&dialog);
    width->setRange(1, 20000);
    height->setRange(1, 20000);
    width->setValue(m_project->canvasSize().width());
    height->setValue(m_project->canvasSize().height());
    form->addRow("Width (px)", width);
    form->addRow("Height (px)", height);
    auto* centered = new QCheckBox("Keep artwork centered", &dialog);
    centered->setChecked(true);
    form->addRow(centered);
    form->addRow(new QLabel("Changes canvas bounds only; layers are not resampled or deleted.", &dialog));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    const QSize size(width->value(), height->value());
    if (size == m_project->canvasSize()) return;
    m_undoStack->push(new ResizeCanvasCommand(m_project.get(), size, centered->isChecked()));
    m_canvas->zoomToFit();
    m_canvas->update();
}


void MainWindow::showLayerContextMenu(Layer* layer, QPoint globalPosition) {
    if (m_project->indexOf(layer) < 0) layer = nullptr;
    m_canvas->setSelectedLayer(layer);
    m_layersPanel->selectLayer(layer);
    updateSelection(layer);

    QMenu menu(this);
    menu.setObjectName("layerContextMenu");
    // A history change invalidates the menu's snapshot (and potentially its layer).
    connect(m_undoStack, &QUndoStack::indexChanged, &menu, &QMenu::close);
    menu.addAction(m_undoStack->createUndoAction(&menu, "Undo"));
    menu.addAction(m_undoStack->createRedoAction(&menu, "Redo"));
    menu.addSeparator();
    auto* copy = menu.addAction("Copy Layer", this, [this, layer] { onCopyLayer(layer); });
    copy->setEnabled(layer != nullptr);
    auto* paste = menu.addAction("Paste Layer", this, &MainWindow::onPasteLayer);
    paste->setEnabled(m_layerClipboard != nullptr);
    auto* systemPaste = menu.addAction("Paste Image from System Clipboard", this,
                                      &MainWindow::onPasteImageFromSystemClipboard);
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    systemPaste->setEnabled(mime && mime->hasImage());
    menu.addSeparator();
    auto* duplicate = menu.addAction("Duplicate Layer", this, [this, layer] { onDuplicateLayer(layer); });
    duplicate->setEnabled(layer != nullptr);
    auto* remove = menu.addAction("Delete Layer", this, [this, layer] { onDeleteLayer(layer); });
    remove->setEnabled(layer != nullptr);
    auto* visibility = menu.addAction(layer && !layer->visible ? "Show Layer" : "Hide Layer", this,
        [this, layer] { if (layer) onLayerVisibilityToggled(layer, !layer->visible); });
    visibility->setEnabled(layer != nullptr);
    auto* up = menu.addAction("Move Layer Up", this, [this, layer] { onMoveLayerUp(layer); });
    auto* down = menu.addAction("Move Layer Down", this, [this, layer] { onMoveLayerDown(layer); });
    const int index = m_project->indexOf(layer);
    up->setEnabled(index >= 0 && index < m_project->layerCount() - 1);
    down->setEnabled(index > 0);

    auto* transform = menu.addMenu("Transform");
    transform->setEnabled(layer && !layer->locked);
    transform->addAction("Flip Horizontal", this, &MainWindow::onFlipHorizontal);
    transform->addAction("Flip Vertical", this, &MainWindow::onFlipVertical);
    transform->addAction("Rotate 90° CW", this, [this] { onRotate90(true); });
    transform->addAction("Rotate 90° CCW", this, [this] { onRotate90(false); });
    transform->addAction("Reset Transform", this, &MainWindow::onResetTransform);

    menu.addSeparator();
    menu.addAction("Import Image...", this, &MainWindow::onImportImage);
    menu.addAction("Add Text Layer...", this, &MainWindow::onAddTextLayer);
    menu.addAction("Add Shape Layer", this, &MainWindow::onAddShapeLayer);
    menu.addSeparator();
    menu.addAction("Zoom to Fit", this, [this] { m_canvas->zoomToFit(); m_canvas->update(); });
    menu.exec(globalPosition);
}
