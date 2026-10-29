#include "MainWindow.h"
#include "../project/Commands.h"
#include "../project/TextLayer.h"
#include "../project/ShapeLayer.h"
#include "../engine/Transform.h"
#include "../io/ImageIO.h"
#include "../io/ProjectIO.h"

#include <QMenuBar>
#include <QMenu>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QToolBar>
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
#include <QVBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QStatusBar>
#include <QSpinBox>
#include <QTimer>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_project = std::make_unique<Project>(QSize(1500, 1028));
    m_undoStack = new QUndoStack(this);
    loadStarterProject();

    auto* workspace = new QWidget(this);
    auto* workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);
    m_documentLabel = new QLabel(workspace);
    m_documentLabel->setObjectName("documentTab");
    m_documentLabel->setMinimumHeight(35);
    workspaceLayout->addWidget(m_documentLabel);
    m_canvas = new Canvas(workspace);
    m_canvas->setProject(m_project.get());
    m_canvas->setUndoStack(m_undoStack);
    workspaceLayout->addWidget(m_canvas, 1);

    m_layersPanel = new LayersPanel(this);
    m_propertiesPanel = new PropertiesPanel(this);
    m_propertiesPanel->setUndoStack(m_undoStack);
    m_photoPanel = new PhotoAdjustmentsPanel(this);
    m_photoPanel->setUndoStack(m_undoStack);
    m_inspectorTabs = new QTabWidget(this);
    m_inspectorTabs->addTab(m_propertiesPanel, "Properties");
    m_inspectorTabs->addTab(m_photoPanel, "Adjustments");
    m_inspectorTabs->setCurrentWidget(m_photoPanel);
    auto* sidebar = new QSplitter(Qt::Vertical, this);
    sidebar->setChildrenCollapsible(false);
    sidebar->addWidget(m_inspectorTabs);
    sidebar->addWidget(m_layersPanel);
    sidebar->setSizes({520, 320});
    sidebar->setMinimumWidth(280);
    sidebar->setMaximumWidth(400);
    auto* mainSplit = new QSplitter(Qt::Horizontal, this);
    mainSplit->addWidget(workspace);
    mainSplit->addWidget(sidebar);
    mainSplit->setStretchFactor(0, 1);
    mainSplit->setStretchFactor(1, 0);
    setCentralWidget(mainSplit);

    m_statusLabel = new QLabel(this);
    m_zoomLabel = new QLabel(this);
    statusBar()->addWidget(m_zoomLabel);
    statusBar()->addWidget(m_statusLabel, 1);
    syncPanelsFromProject();
    buildMenusAndToolbar();

    connect(m_canvas, &Canvas::selectionChanged, this, &MainWindow::onCanvasSelectionChanged);
    connect(m_canvas, &Canvas::transformLiveChanged, this, [this](Layer*) {
        m_propertiesPanel->refreshFromLayer();
    });
    connect(m_canvas, &Canvas::zoomChanged, this, [this](double) { refreshStatus(); });
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
    // Commands may remove the selected layer. Never leave borrowed pointers in
    // the canvas or inspectors after undo/redo, and refresh the stack view.
    connect(m_undoStack, &QUndoStack::indexChanged, this, [this] {
        Layer* selected = m_canvas->selectedLayer();
        if (selected && m_project->indexOf(selected) < 0) {
            m_canvas->setSelectedLayer(nullptr);
            selected = nullptr;
        }
        m_canvas->update();
        refreshTitle();
        refreshStatus();
        // Defer list rebuilding: a visibility command can originate inside
        // QListWidget::itemChanged, whose item must remain alive until it returns.
        QTimer::singleShot(0, this, [this] {
            m_layersPanel->refresh();
            m_layersPanel->selectLayer(m_canvas->selectedLayer());
            m_propertiesPanel->refreshFromLayer();
            m_photoPanel->refreshFromLayer();
        });
    });
    m_canvas->setSelectedLayer(m_project->layerAt(0));
    m_layersPanel->selectLayer(m_project->layerAt(0));
    updateSelection(m_project->layerAt(0));
    refreshTitle();
    refreshStatus();
    QTimer::singleShot(0, m_canvas, &Canvas::zoomToFit);
}

void MainWindow::loadStarterProject() {
    QImage image(":/assets/great-wave.jpg");
    if (image.isNull()) return;
    m_project->setCanvasSize(image.size());
    auto bg = std::make_unique<Layer>(LayerType::Raster, "Background");
    bg->content = image;
    bg->transform.center = QPointF(image.width()/2.0, image.height()/2.0);
    m_project->insertLayer(std::move(bg));
    auto card = std::make_unique<ShapeLayer>(ShapeKind::RoundedRectangle, "Caption Card");
    card->localSize = QSize(590, 165);
    card->cornerRadius = 16;
    card->fillColor = QColor("#172738");
    card->opacity = 0.90;
    card->regenerate();
    card->transform.center = QPointF(92 + 295, 746 + 82.5);
    m_project->insertLayer(std::move(card));
    auto title = std::make_unique<TextLayer>("The Great Wave", "Title");
    title->font = QFont("Sans Serif", 48, QFont::Bold);
    title->color = QColor("#f7f5ec");
    title->regenerate();
    title->transform.center = QPointF(125 + title->content.width()/2.0, 765 + title->content.height()/2.0);
    m_project->insertLayer(std::move(title));
    auto credit = std::make_unique<TextLayer>("Katsushika Hokusai  ·  c. 1831  ·  public domain", "Credit");
    credit->font = QFont("Sans Serif", 16);
    credit->color = QColor("#dbe5eb");
    credit->regenerate();
    credit->transform.center = QPointF(128 + credit->content.width()/2.0, 855 + credit->content.height()/2.0);
    m_project->insertLayer(std::move(credit));
    m_documentName = "great-wave.jpg";
}

void MainWindow::buildMenusAndToolbar() {
    QMenu* file = menuBar()->addMenu("&File");
    file->addAction("New Project…", this, &MainWindow::onNewProject, QKeySequence::New);
    file->addAction("Open Image…", this, &MainWindow::onOpenImage, QKeySequence::Open);
    file->addAction("Open Project…", this, &MainWindow::onOpenProject, QKeySequence("Ctrl+Shift+O"));
    file->addSeparator();
    file->addAction("Save Project", this, &MainWindow::onSaveProject, QKeySequence::Save);
    file->addAction("Save Project As…", this, &MainWindow::onSaveProjectAs, QKeySequence::SaveAs);
    file->addAction("Import Image as Layer…", this, &MainWindow::onImportImage, QKeySequence("Ctrl+I"));
    file->addAction("Export Image…", this, &MainWindow::onExportImage, QKeySequence("Ctrl+E"));
    file->addSeparator();
    file->addAction("Quit", this, &QWidget::close, QKeySequence::Quit);
    QMenu* edit = menuBar()->addMenu("&Edit");
    QAction* undo = m_undoStack->createUndoAction(this, "Undo"); undo->setShortcut(QKeySequence::Undo); edit->addAction(undo);
    QAction* redo = m_undoStack->createRedoAction(this, "Redo"); redo->setShortcut(QKeySequence::Redo); edit->addAction(redo);
    edit->addSeparator();
    auto* copy = edit->addAction("Copy Layer", QKeySequence::Copy);
    connect(copy, &QAction::triggered, this, [this] { onCopyLayer(m_canvas->selectedLayer()); });
    edit->addAction("Paste Layer", this, &MainWindow::onPasteLayer, QKeySequence::Paste);
    edit->addAction("Copy Image to Clipboard", this, &MainWindow::onCopyLayerToSystemClipboard, QKeySequence("Ctrl+Shift+C"));
    edit->addAction("Paste Image from Clipboard", this, &MainWindow::onPasteImageFromSystemClipboard, QKeySequence("Ctrl+Shift+V"));
    QMenu* image = menuBar()->addMenu("&Image");
    image->addAction("Crop Canvas…", this, &MainWindow::onCropCanvas);
    image->addAction("Flip Horizontal", this, &MainWindow::onFlipHorizontal);
    image->addAction("Flip Vertical", this, &MainWindow::onFlipVertical);
    auto* cw = image->addAction("Rotate 90° Clockwise"); connect(cw, &QAction::triggered, this, [this]{ onRotate90(true); });
    auto* ccw = image->addAction("Rotate 90° Counterclockwise"); connect(ccw, &QAction::triggered, this, [this]{ onRotate90(false); });
    image->addAction("Reset Transform", this, &MainWindow::onResetTransform);
    QMenu* layer = menuBar()->addMenu("&Layer");
    layer->addAction("Add Image…", this, &MainWindow::onAddRasterLayer);
    layer->addAction("Add Text…", this, &MainWindow::onAddTextLayer);
    layer->addAction("Add Shape", this, &MainWindow::onAddShapeLayer);
    layer->addSeparator();
    auto* del = layer->addAction("Delete Selected Layer"); del->setShortcut(QKeySequence::Delete); connect(del, &QAction::triggered, this, [this]{ onDeleteLayer(m_canvas->selectedLayer()); });
    QMenu* view = menuBar()->addMenu("&View");
    view->addAction("Fit to Screen", this, [this]{ m_canvas->zoomToFit(); }, QKeySequence("Ctrl+0"));
    view->addAction("Zoom In", this, [this]{ m_canvas->setZoom(m_canvas->zoom()*1.25); }, QKeySequence("Ctrl++"));
    view->addAction("Zoom Out", this, [this]{ m_canvas->setZoom(m_canvas->zoom()/1.25); }, QKeySequence("Ctrl+-"));

    QToolBar* options = addToolBar("Options");
    options->setMovable(false);
    options->addWidget(new QLabel("  MOVE / TRANSFORM     ", options));
    options->addSeparator();
    options->addAction("Open Image", this, &MainWindow::onOpenImage);
    options->addAction("Save Project", this, &MainWindow::onSaveProject);
    options->addAction("Export", this, &MainWindow::onExportImage);
    options->addSeparator();
    options->addAction(undo); options->addAction(redo);
    options->addSeparator();
    options->addAction("Fit", this, [this]{ m_canvas->zoomToFit(); });
    options->addAction("−", this, [this]{ m_canvas->setZoom(m_canvas->zoom()/1.25); });
    options->addAction("+", this, [this]{ m_canvas->setZoom(m_canvas->zoom()*1.25); });

    QToolBar* tools = new QToolBar("Tools", this);
    tools->setObjectName("toolsBar");
    tools->setMovable(false);
    addToolBar(Qt::LeftToolBarArea, tools);
    auto* group = new QActionGroup(this); group->setExclusive(true);
    auto* move = tools->addAction("✥"); move->setToolTip("Move and transform layers"); move->setCheckable(true); move->setChecked(true); group->addAction(move);
    connect(move, &QAction::triggered, this, [this]{ m_canvas->setTool(ToolMode::Select); });
    auto* pan = tools->addAction("✋"); pan->setToolTip("Pan canvas"); pan->setCheckable(true); group->addAction(pan);
    connect(pan, &QAction::triggered, this, [this]{ m_canvas->setTool(ToolMode::Pan); });
    tools->addSeparator();
    auto* addImage = tools->addAction("▧"); addImage->setToolTip("Import image layer"); connect(addImage, &QAction::triggered, this, &MainWindow::onImportImage);
    auto* addText = tools->addAction("T"); addText->setToolTip("Add text layer"); connect(addText, &QAction::triggered, this, &MainWindow::onAddTextLayer);
    auto* addShape = tools->addAction("▱"); addShape->setToolTip("Add shape layer"); connect(addShape, &QAction::triggered, this, &MainWindow::onAddShapeLayer);
}

void MainWindow::refreshTitle() {
    QString name = m_project->filePath.isEmpty() ? m_documentName : QFileInfo(m_project->filePath).fileName();
    if (name.isEmpty()) name = "Untitled";
    const QString dirty = m_undoStack->isClean() ? "" : " *";
    setWindowTitle(name + dirty + " — NexImageEditor");
    m_documentLabel->setText("   " + name + dirty + "    ·    RGB / 8");
}

void MainWindow::refreshStatus() {
    m_zoomLabel->setText(QString(" %1% ").arg(m_canvas->zoom()*100, 0, 'f', 1));
    m_statusLabel->setText(QString("  RGB Color · 8 bit     |     %1 × %2 px     |     %3 layers")
        .arg(m_project->canvasSize().width()).arg(m_project->canvasSize().height()).arg(m_project->layerCount()));
}

void MainWindow::syncPanelsFromProject() {
    m_layersPanel->setProject(m_project.get());
    m_propertiesPanel->setProject(m_project.get());
    m_photoPanel->setProject(m_project.get());
}

void MainWindow::updateSelection(Layer* layer) {
    m_propertiesPanel->setLayer(layer);
    m_photoPanel->setLayer(layer);
}

bool MainWindow::confirmDiscardChanges() {
    if (m_undoStack->isClean()) return true;
    auto answer = QMessageBox::warning(this, "Unsaved changes", "Save changes to the current project?",
                                       QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) { onSaveProject(); return m_undoStack->isClean(); }
    return true;
}
void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirmDiscardChanges()) event->accept(); else event->ignore();
}

// ---------------- File actions ----------------

void MainWindow::onNewProject() {
    if (!confirmDiscardChanges()) return;
    bool ok = false;
    int w = QInputDialog::getInt(this, "New Project", "Width:", 1920, 1, 20000, 1, &ok);
    if (!ok) return;
    int h = QInputDialog::getInt(this, "New Project", "Height:", 1080, 1, 20000, 1, &ok);
    if (!ok) return;

    m_canvas->setSelectedLayer(nullptr);
    m_undoStack->clear();
    m_project = std::make_unique<Project>(QSize(w, h));
    m_documentName = "Untitled";
    m_canvas->setProject(m_project.get());
    syncPanelsFromProject();
    m_layersPanel->refresh();
    updateSelection(nullptr);
    refreshTitle();
    refreshStatus();
    m_canvas->zoomToFit();
}

void MainWindow::onOpenProject() {
    if (!confirmDiscardChanges()) return;
    QString path = QFileDialog::getOpenFileName(this, "Open Project", QString(), "NexImageEditor Project (*.nxproj)");
    if (path.isEmpty()) return;

    auto loaded = std::make_unique<Project>();
    if (!ProjectIO::loadProject(path, *loaded)) {
        QMessageBox::warning(this, "Open Project", "Could not read this project file.");
        return;
    }
    m_canvas->setSelectedLayer(nullptr);
    m_undoStack->clear();
    m_project = std::move(loaded);
    m_documentName = QFileInfo(path).fileName();
    m_canvas->setProject(m_project.get());
    syncPanelsFromProject();
    m_layersPanel->refresh();
    updateSelection(nullptr);
    refreshTitle();
    refreshStatus();
    m_canvas->zoomToFit();
}

void MainWindow::onSaveProject() {
    if (m_project->filePath.isEmpty()) { onSaveProjectAs(); return; }
    if (!ProjectIO::saveProject(m_project->filePath, *m_project)) {
        QMessageBox::warning(this, "Save Project", "Could not save the project.");
        return;
    }
    m_undoStack->setClean();
    refreshTitle();
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

void MainWindow::onOpenImage() {
    if (!confirmDiscardChanges()) return;
    QString path = QFileDialog::getOpenFileName(this, "Open Image", QString(),
        "Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff)");
    if (path.isEmpty()) return;
    ImageBuffer image = ImageIO::loadImage(path);
    if (image.isNull()) {
        QMessageBox::warning(this, "Open Image", "Could not read this image file.");
        return;
    }
    m_canvas->setSelectedLayer(nullptr);
    m_undoStack->clear();
    m_project = std::make_unique<Project>(image.image().size());
    m_documentName = QFileInfo(path).fileName();
    auto layer = std::make_unique<Layer>(LayerType::Raster, "Background");
    layer->content = image.image();
    layer->transform.center = QPointF(image.image().width()/2.0, image.image().height()/2.0);
    Layer* raw = m_project->insertLayer(std::move(layer));
    m_canvas->setProject(m_project.get());
    syncPanelsFromProject();
    m_canvas->setSelectedLayer(raw);
    m_layersPanel->selectLayer(raw);
    updateSelection(raw);
    m_canvas->zoomToFit();
    refreshTitle();
    refreshStatus();
}

void MainWindow::onCropCanvas() {
    QDialog dialog(this);
    dialog.setWindowTitle("Crop Canvas");
    QFormLayout form(&dialog);
    QSpinBox x, y, width, height;
    QSize size = m_project->canvasSize();
    x.setRange(0, size.width()-1); y.setRange(0, size.height()-1);
    width.setRange(1, size.width()); height.setRange(1, size.height());
    width.setValue(size.width()); height.setValue(size.height());
    connect(&x, qOverload<int>(&QSpinBox::valueChanged), &dialog, [&width, size](int v) { width.setMaximum(size.width()-v); });
    connect(&y, qOverload<int>(&QSpinBox::valueChanged), &dialog, [&height, size](int v) { height.setMaximum(size.height()-v); });
    form.addRow("Left (px)", &x); form.addRow("Top (px)", &y);
    form.addRow("Width (px)", &width); form.addRow("Height (px)", &height);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    QRect crop(x.value(), y.value(), width.value(), height.value());
    if (crop == QRect(QPoint(0, 0), size)) return;
    m_undoStack->push(new CropCanvasCommand(m_project.get(), crop));
    m_canvas->zoomToFit();
}

void MainWindow::onImportImage() {
    QString path = QFileDialog::getOpenFileName(this, "Import Image", QString(),
                                                 "Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff)");
    if (path.isEmpty()) return;

    ImageBuffer buf = ImageIO::loadImage(path);
    if (buf.isNull()) {
        QMessageBox::warning(this, "Import Image", "Could not read this image file.");
        return;
    }

    auto layer = std::make_unique<Layer>(LayerType::Raster, QFileInfo(path).baseName());
    layer->content = buf.image();
    // Center the imported image on the canvas by default.
    layer->transform.center = QPointF(m_project->canvasSize().width() / 2.0,
                                       m_project->canvasSize().height() / 2.0);

    Layer* raw = layer.get();
    auto* cmd = new AddLayerCommand(m_project.get(), std::move(layer));
    m_undoStack->push(cmd);
    m_canvas->setSelectedLayer(raw);
    m_layersPanel->selectLayer(raw);
    updateSelection(raw);
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
    Layer* raw = layer.get();
    auto* cmd = new AddLayerCommand(m_project.get(), std::move(layer));
    m_undoStack->push(cmd);
    m_canvas->setSelectedLayer(raw);
    m_layersPanel->selectLayer(raw);
    updateSelection(raw);
}

void MainWindow::onAddShapeLayer() {
    auto layer = std::make_unique<ShapeLayer>(ShapeKind::Rectangle);
    layer->transform.center = QPointF(m_project->canvasSize().width() / 2.0,
                                       m_project->canvasSize().height() / 2.0);
    Layer* raw = layer.get();
    auto* cmd = new AddLayerCommand(m_project.get(), std::move(layer));
    m_undoStack->push(cmd);
    m_canvas->setSelectedLayer(raw);
    m_layersPanel->selectLayer(raw);
    updateSelection(raw);
}

void MainWindow::onDeleteLayer(Layer* layer) {
    if (!layer) return;
    int idx = m_project->indexOf(layer);
    if (idx < 0) return;
    if (m_canvas->selectedLayer() == layer) m_canvas->setSelectedLayer(nullptr);
    m_undoStack->push(new RemoveLayerCommand(m_project.get(), idx));
    m_layersPanel->refresh();
    updateSelection(nullptr);
}

void MainWindow::onDuplicateLayer(Layer* layer) {
    if (!layer) return;
    auto copy = layer->clone();
    copy->name += " copy";
    copy->transform.center += QPointF(20, 20);
    Layer* raw = copy.get();
    auto* cmd = new AddLayerCommand(m_project.get(), std::move(copy));
    m_undoStack->push(cmd);
    m_canvas->setSelectedLayer(raw);
    m_layersPanel->selectLayer(raw);
    updateSelection(raw);
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
    m_layersPanel->refresh();
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
    m_layersPanel->refresh();
    m_canvas->setSelectedLayer(raw);
    m_layersPanel->selectLayer(raw);
    updateSelection(raw);
}

void MainWindow::onMoveLayerUp(Layer* layer) {
    if (!layer) return;
    int idx = m_project->indexOf(layer);
    if (idx < 0 || idx == m_project->layerCount() - 1) return;
    m_undoStack->push(new MoveLayerCommand(m_project.get(), idx, idx + 1));
    m_layersPanel->refresh();
    m_layersPanel->selectLayer(layer);
}

void MainWindow::onMoveLayerDown(Layer* layer) {
    if (!layer) return;
    int idx = m_project->indexOf(layer);
    if (idx <= 0) return;
    m_undoStack->push(new MoveLayerCommand(m_project.get(), idx, idx - 1));
    m_layersPanel->refresh();
    m_layersPanel->selectLayer(layer);
}

void MainWindow::onLayerVisibilityToggled(Layer* layer, bool visible) {
    if (!layer) return;
    if (layer->visible == visible) return;
    m_undoStack->push(new SetLayerPropertyCommand<bool>(m_project.get(), layer->id(), layer->visible, visible,
        [](Layer* l) { return l->visible; }, [](Layer* l, bool v) { l->visible = v; }, "Toggle Layer Visibility"));
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
