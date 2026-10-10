#include "ui/MainWindow.h"

#include "core/Adjustments.h"
#include "core/FileIO.h"
#include "core/Filters.h"
#include "core/Ops.h"
#include "core/Settings.h"
#include "ui/CanvasView.h"
#include "ui/Dialogs.h"
#include "ui/Icons.h"
#include "ui/Panels.h"

#include <QAction>
#include <QObject>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QPainter>
#include <QUndoStack>
#include <QUndoView>
#include <QFont>
#include <utility>

namespace {
// Adds a menu action with a shortcut and connects it to a slot or functor.
template <typename Recv, typename Fn>
QAction* menuAct(QMenu* m, const QString& text, const QKeySequence& key, const Recv* r, Fn fn) {
    QAction* a = m->addAction(text);
    a->setShortcut(key);
    QObject::connect(a, &QAction::triggered, r, fn);
    return a;
}
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("NexImage Editor");
    resize(1440, 900);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    setCentralWidget(m_tabs);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int) {
        CanvasView* v = currentView();
        m_layers->setView(v);
        m_props->setView(v);
        m_history->setView(v);
        showOptionsFor(v ? v->toolId() : ToolId::Move);
        updateUi();
    });

    m_layers = new LayersPanel(this);
    m_props = new PropertiesPanel(this);
    m_history = new HistoryPanel(this);
    auto* layersDock = new QDockWidget("Layers", this);
    layersDock->setWidget(m_layers);
    auto* propsDock = new QDockWidget("Properties", this);
    propsDock->setWidget(m_props);
    auto* historyDock = new QDockWidget("History", this);
    historyDock->setWidget(m_history);
    addDockWidget(Qt::RightDockWidgetArea, layersDock);
    addDockWidget(Qt::RightDockWidgetArea, propsDock);
    addDockWidget(Qt::RightDockWidgetArea, historyDock);
    tabifyDockWidget(layersDock, historyDock);
    layersDock->raise();

    m_zoomLabel = new QLabel(this);
    m_sizeLabel = new QLabel(this);
    m_posLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_posLabel);
    statusBar()->addPermanentWidget(m_sizeLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);

    buildToolBar();
    buildOptionsBar();
    buildMenus();
    updateOptionsBar();
    showOptionsFor(ToolId::Move);
    updateUi();
    addDocument(Document(1920, 1080, Qt::white, "Untitled-1"));
}

// ---------------------------------------------------------------- Setup

void MainWindow::buildToolBar() {
    m_toolBar = addToolBar("Tools");
    m_toolBar->setOrientation(Qt::Vertical);
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(QSize(22, 22));
    addToolBar(Qt::LeftToolBarArea, m_toolBar);

    auto* group = new QActionGroup(this);
    group->setExclusive(true);
    struct Entry { ToolId id; const char* key; bool sep; };
    const Entry entries[] = {
        {ToolId::Move, "V", false},
        {ToolId::RectMarquee, "M", true},
        {ToolId::EllipseMarquee, "Shift+M", false},
        {ToolId::Lasso, "L", false},
        {ToolId::MagicWand, "W", false},
        {ToolId::Crop, "C", false},
        {ToolId::Eyedropper, "I", false},
        {ToolId::Brush, "B", true},
        {ToolId::Eraser, "E", false},
        {ToolId::Bucket, "G", true},
        {ToolId::Gradient, "Shift+G", false},
        {ToolId::Text, "T", true},
        {ToolId::ShapeRect, "U", false},
        {ToolId::ShapeEllipse, "Shift+U", false},
        {ToolId::Transform, "Ctrl+T", true},
        {ToolId::Zoom, "Z", false},
        {ToolId::Hand, "H", false},
    };
    for (const Entry& e : entries) {
        if (e.sep) m_toolBar->addSeparator();
        QAction* a = m_toolBar->addAction(Icons::tool(e.id), toolName(e.id));
        a->setCheckable(true);
        a->setShortcut(QKeySequence(e.key));
        a->setToolTip(QString("%1 (%2)").arg(toolName(e.id), e.key));
        group->addAction(a);
        m_toolActions.insert(e.id, a);
        const ToolId id = e.id;
        connect(a, &QAction::triggered, this, [this, id] {
            if (CanvasView* v = currentView()) v->setTool(id);
        });
    }
}

void MainWindow::buildOptionsBar() {
    m_optionsBar = addToolBar("Options");
    m_optionsBar->setMovable(false);
    addToolBarBreak(Qt::TopToolBarArea);
    addToolBar(Qt::TopToolBarArea, m_optionsBar);

    m_fgBtn = new QPushButton(this);
    m_bgBtn = new QPushButton(this);
    for (QPushButton* b : {m_fgBtn, m_bgBtn}) b->setFixedSize(34, 22);
    auto* swap = new QPushButton("⇄", this);
    swap->setFixedWidth(28);
    swap->setToolTip("Swap colours (X)");
    auto* reset = new QPushButton("D", this);
    reset->setFixedWidth(28);
    reset->setToolTip("Default colours (D)");
    connect(m_fgBtn, &QPushButton::clicked, this, [this] { pickColor(false); });
    connect(m_bgBtn, &QPushButton::clicked, this, [this] { pickColor(true); });
    connect(swap, &QPushButton::clicked, this, [this] {
        std::swap(m_opts.fg, m_opts.bg);
        updateOptionsBar();
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        m_opts.fg = Qt::black;
        m_opts.bg = Qt::white;
        updateOptionsBar();
    });
    auto* swapAct = new QAction(this);
    swapAct->setShortcut(QKeySequence("X"));
    connect(swapAct, &QAction::triggered, swap, &QPushButton::click);
    auto* resetAct = new QAction(this);
    resetAct->setShortcut(QKeySequence("D"));
    connect(resetAct, &QAction::triggered, reset, &QPushButton::click);
    addAction(swapAct);
    addAction(resetAct);

    m_brushSize = new QSpinBox(this);
    m_brushSize->setRange(1, 500);
    m_brushSize->setSuffix(" px");
    connect(m_brushSize, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) { m_opts.brushSize = v; });
    m_hardness = new QSlider(Qt::Horizontal, this);
    m_hardness->setRange(0, 100);
    m_hardness->setFixedWidth(120);
    m_hardness->setToolTip("Hardness");
    connect(m_hardness, &QSlider::valueChanged, this, [this](int v) { m_opts.hardness = v / 100.0; });
    m_tolerance = new QSpinBox(this);
    m_tolerance->setRange(0, 255);
    m_tolerance->setPrefix("Tolerance ");
    connect(m_tolerance, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) { m_opts.tolerance = v; });
    m_contiguous = new QCheckBox("Contiguous", this);
    connect(m_contiguous, &QCheckBox::toggled, this, [this](bool on) { m_opts.contiguous = on; });
    m_font = new QFontComboBox(this);
    m_font->setFixedWidth(160);
    connect(m_font, &QFontComboBox::currentFontChanged, this, [this](const QFont& f) { m_opts.fontFamily = f.family(); });
    m_fontSize = new QSpinBox(this);
    m_fontSize->setRange(4, 800);
    m_fontSize->setSuffix(" pt");
    m_fontSize->setValue(m_opts.fontSize);
    connect(m_fontSize, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) { m_opts.fontSize = v; });
    m_shapeStroke = new QCheckBox("Stroke only", this);
    connect(m_shapeStroke, &QCheckBox::toggled, this, [this](bool on) { m_opts.shapeStroke = on; });
    m_strokeWidth = new QSpinBox(this);
    m_strokeWidth->setRange(1, 200);
    m_strokeWidth->setPrefix("Stroke ");
    m_strokeWidth->setSuffix(" px");
    connect(m_strokeWidth, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) { m_opts.strokeWidth = v; });

    m_optionsBar->addWidget(m_fgBtn);
    m_optionsBar->addWidget(m_bgBtn);
    m_optionsBar->addWidget(swap);
    m_optionsBar->addWidget(reset);
    m_optionsBar->addSeparator();
    m_brushWidgets = {m_brushSize, m_hardness};
    m_wandWidgets = {m_tolerance, m_contiguous};
    m_textWidgets = {m_font, m_fontSize};
    m_shapeWidgets = {m_shapeStroke, m_strokeWidth};
    for (QWidget* w : QList<QWidget*>{m_brushSize, m_hardness, m_tolerance, m_contiguous, m_font, m_fontSize,
                                      m_shapeStroke, m_strokeWidth})
        m_optionsBar->addWidget(w);
}

void MainWindow::showOptionsFor(ToolId id) {
    const bool isBrush = id == ToolId::Brush || id == ToolId::Eraser;
    const bool isWand = id == ToolId::MagicWand || id == ToolId::Bucket;
    const bool isText = id == ToolId::Text;
    const bool isShape = id == ToolId::ShapeRect || id == ToolId::ShapeEllipse;
    for (QWidget* w : m_brushWidgets) w->setVisible(isBrush);
    for (QWidget* w : m_wandWidgets) w->setVisible(isWand);
    for (QWidget* w : m_textWidgets) w->setVisible(isText);
    for (QWidget* w : m_shapeWidgets) w->setVisible(isShape);
}

void MainWindow::updateOptionsBar() {
    auto swatch = [](QPushButton* b, const QColor& c) {
        b->setStyleSheet(QString("background:%1; border:1px solid #111;").arg(c.name()));
        b->setToolTip(c.name());
    };
    swatch(m_fgBtn, m_opts.fg);
    swatch(m_bgBtn, m_opts.bg);
    m_brushSize->setValue(int(m_opts.brushSize));
    m_hardness->setValue(int(std::lround(m_opts.hardness * 100)));
    m_tolerance->setValue(m_opts.tolerance);
    m_contiguous->setChecked(m_opts.contiguous);
    m_font->setCurrentFont(QFont(m_opts.fontFamily));
    m_fontSize->setValue(m_opts.fontSize);
    m_shapeStroke->setChecked(m_opts.shapeStroke);
    m_strokeWidth->setValue(int(m_opts.strokeWidth));
}

void MainWindow::pickColor(bool background) {
    QColor& target = background ? m_opts.bg : m_opts.fg;
    QColor c = QColorDialog::getColor(target, this, background ? "Background colour" : "Foreground colour");
    if (!c.isValid()) return;
    target = c;
    updateOptionsBar();
}

void MainWindow::buildMenus() {
    QMenu* file = menuBar()->addMenu("&File");
    menuAct(file, "&New…", QKeySequence::New, this, &MainWindow::newDocument);
    menuAct(file, "&Open…", QKeySequence::Open, this, &MainWindow::openDocuments);
    m_recentMenu = file->addMenu("Open &Recent");
    file->addSeparator();
    m_saveAct = menuAct(file, "&Save Project…", QKeySequence::Save, this, [this] {
        if (CanvasView* v = currentView()) saveDocument(v, false);
    });
    menuAct(file, "Save Project &As…", QKeySequence::SaveAs, this, [this] {
        if (CanvasView* v = currentView()) saveDocument(v, true);
    });
    menuAct(file, "&Export As Image…", QKeySequence("Ctrl+Shift+E"), this, &MainWindow::exportDocument);
    file->addSeparator();
    m_closeAct = menuAct(file, "&Close Tab", QKeySequence::Close, this, [this] { closeTab(m_tabs->currentIndex()); });
    menuAct(file, "E&xit", QKeySequence::Quit, this, &QWidget::close);
    updateRecentMenu();

    QMenu* edit = menuBar()->addMenu("&Edit");
    m_undoAct = menuAct(edit, "&Undo", QKeySequence::Undo, this, [this] {
        if (CanvasView* v = currentView()) v->undoStack()->undo();
    });
    m_redoAct = menuAct(edit, "&Redo", QKeySequence::Redo, this, [this] {
        if (CanvasView* v = currentView()) v->undoStack()->redo();
    });
    m_redoAct->setShortcuts({QKeySequence::Redo, QKeySequence("Ctrl+Y")});
    edit->addSeparator();
    menuAct(edit, "&Copy", QKeySequence::Copy, this, &MainWindow::copySelection);
    menuAct(edit, "&Paste as New Layer", QKeySequence::Paste, this, &MainWindow::pasteImage);
    edit->addSeparator();
    menuAct(edit, "Select &All", QKeySequence::SelectAll, this, &MainWindow::selectAll);
    menuAct(edit, "&Deselect", QKeySequence("Ctrl+D"), this, &MainWindow::deselect);
    menuAct(edit, "&Invert Selection", QKeySequence("Ctrl+Shift+I"), this, &MainWindow::invertSelection);
    menuAct(edit, "&Fill Selection with Foreground", QKeySequence("Alt+Backspace"), this, &MainWindow::fillSelection);

    QMenu* image = menuBar()->addMenu("&Image");
    menuAct(image, "Image &Size…", QKeySequence("Ctrl+Alt+I"), this, &MainWindow::imageSize);
    menuAct(image, "&Canvas Size…", QKeySequence("Ctrl+Alt+C"), this, &MainWindow::canvasSize);
    QMenu* rot = image->addMenu("&Rotate");
    rot->addAction("90° Clockwise", this, [this] { rotate(90); });
    rot->addAction("90° Counter-clockwise", this, [this] { rotate(-90); });
    rot->addAction("180°", this, [this] { rotate(180); });
    QMenu* flipMenu = image->addMenu("&Flip");
    flipMenu->addAction("Horizontal", this, [this] { flip(true); });
    flipMenu->addAction("Vertical", this, [this] { flip(false); });

    QMenu* layer = menuBar()->addMenu("&Layer");
    menuAct(layer, "&New Layer", QKeySequence("Ctrl+Shift+N"), this, [this] { if (auto* v = currentView()) v->addLayer(); });
    menuAct(layer, "&Duplicate Layer", QKeySequence("Ctrl+J"), this, [this] { if (auto* v = currentView()) v->duplicateLayer(); });
    layer->addAction("&Delete Layer", this, [this] { if (auto* v = currentView()) v->deleteLayer(); });
    menuAct(layer, "&Merge Down", QKeySequence("Ctrl+E"), this, [this] { if (auto* v = currentView()) v->mergeDownLayer(); });
    layer->addAction("&Flatten Image", this, [this] { if (auto* v = currentView()) v->flattenImage(); });

    QMenu* adj = menuBar()->addMenu("&Adjustments");
    adj->addAction("&Brightness / Contrast…", this, [this] { adjustments(0); });
    adj->addAction("&Hue / Saturation…", this, [this] { adjustments(1); });
    adj->addAction("&Levels…", this, [this] { adjustments(2); });
    adj->addAction("&Tint…", this, [this] { adjustments(3); });
    adj->addSeparator();
    menuAct(adj, "&Invert", QKeySequence("Ctrl+I"), this, [this] { adjustments(4); });
    menuAct(adj, "&Grayscale", QKeySequence("Ctrl+Shift+U"), this, [this] { adjustments(5); });

    QMenu* filt = menuBar()->addMenu("F&ilter");
    filt->addAction("&Gaussian Blur…", this, [this] { filters(0); });
    filt->addAction("&Unsharp Mask…", this, [this] { filters(1); });
    filt->addAction("&Median…", this, [this] { filters(2); });
    filt->addAction("Add &Noise…", this, [this] { filters(3); });

    QMenu* view = menuBar()->addMenu("&View");
    menuAct(view, "Zoom &In", QKeySequence("Ctrl+="), this, [this] { if (auto* v = currentView()) v->zoomAt(QPointF(v->document().width() / 2.0, v->document().height() / 2.0), true); });
    menuAct(view, "Zoom &Out", QKeySequence("Ctrl+-"), this, [this] { if (auto* v = currentView()) v->zoomAt(QPointF(v->document().width() / 2.0, v->document().height() / 2.0), false); });
    menuAct(view, "&Fit on Screen", QKeySequence("Ctrl+0"), this, [this] { if (auto* v = currentView()) v->fitToWindow(); });
    menuAct(view, "&Actual Pixels", QKeySequence("Ctrl+1"), this, [this] {
        if (auto* v = currentView()) v->setZoom(1.0, QPointF(v->width() / 2.0, v->height() / 2.0));
    });

    QMenu* help = menuBar()->addMenu("&Help");
    help->addAction("&About", this, [this] {
        QMessageBox::about(this, "About NexImage Editor",
                           "NexImage Editor 2.0.0\nC++17 / Qt 6 image editor with a Photoshop-style layout.");
    });
}

void MainWindow::updateRecentMenu() {
    m_recentMenu->clear();
    const QStringList recent = Settings::recentFiles();
    if (recent.isEmpty()) {
        m_recentMenu->addAction("(empty)")->setEnabled(false);
        return;
    }
    for (const QString& path : recent)
        m_recentMenu->addAction(QFileInfo(path).fileName(), this, [this, path] { openPath(path); })
            ->setToolTip(path);
}

// ---------------------------------------------------------------- Documents

CanvasView* MainWindow::currentView() const {
    return qobject_cast<CanvasView*>(m_tabs->currentWidget());
}

CanvasView* MainWindow::addDocument(Document doc) {
    auto* view = new CanvasView(std::move(doc), &m_opts, m_tabs);
    const int idx = m_tabs->addTab(view, view->document().name);
    m_tabs->setCurrentIndex(idx);
    connect(view, &CanvasView::changed, this, [this, view] {
        if (view == currentView()) {
            m_layers->refresh();
            m_props->refresh();
        }
        updateUi();
    });
    connect(view, &CanvasView::statusMessage, statusBar(), [this](const QString& t) { statusBar()->showMessage(t, 5000); });
    connect(view, &CanvasView::pointerMoved, this, [this, view](const QPoint& p) {
        if (view == currentView())
            m_posLabel->setText(QString("X: %1  Y: %2").arg(p.x()).arg(p.y()));
    });
    connect(view, &CanvasView::zoomChanged, this, [this, view](double) { if (view == currentView()) updateUi(); });
    connect(view, &CanvasView::toolChanged, this, [this, view](ToolId id) {
        if (view != currentView()) return;
        if (QAction* a = m_toolActions.value(id)) a->setChecked(true);
        statusBar()->showMessage(view->tool()->hint());
        showOptionsFor(id);
    });
    connect(view->undoStack(), &QUndoStack::cleanChanged, this, [this] { updateTabTitles(); });
    m_layers->setView(view);
    m_props->setView(view);
    m_history->setView(view);
    updateTabTitles();
    updateUi();
    return view;
}

void MainWindow::newDocument() {
    NewDocumentDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    addDocument(Document(dlg.width(), dlg.height(), dlg.background(),
                         QString("Untitled-%1").arg(++m_untitledCount)));
}

void MainWindow::openDocuments() {
    const QStringList paths = QFileDialog::getOpenFileNames(this, "Open", QString(), FileIO::imageOpenFilter());
    for (const QString& p : paths) openPath(p);
}

void MainWindow::openPath(const QString& path) {
    Document doc;
    QString err;
    if (!FileIO::importImage(path, doc, &err)) {
        QMessageBox::warning(this, "Open failed", QString("%1\n\n%2").arg(path, err));
        return;
    }
    Settings::addRecent(path);
    updateRecentMenu();
    CanvasView* v = addDocument(std::move(doc));
    if (!v->document().filePath.isEmpty()) v->undoStack()->setClean();
    statusBar()->showMessage("Opened " + path, 5000);
}

bool MainWindow::saveDocument(CanvasView* view, bool asNew) {
    if (!view) return false;
    Document& d = view->document();
    QString path = d.filePath;
    if (path.isEmpty() || asNew || !path.endsWith(".nxe", Qt::CaseInsensitive)) {
        if (path.isEmpty() || asNew)
            path = QFileDialog::getSaveFileName(this, "Save Project", d.name + ".nxe", "NexImage project (*.nxe)");
        if (path.isEmpty()) return false;
        if (!path.endsWith(".nxe", Qt::CaseInsensitive)) path += ".nxe";
    }
    QString err;
    if (!FileIO::saveProject(d, path, &err)) {
        QMessageBox::warning(this, "Save failed", err);
        return false;
    }
    d.filePath = path;
    d.name = QFileInfo(path).completeBaseName();
    view->undoStack()->setClean();
    Settings::addRecent(path);
    updateRecentMenu();
    updateTabTitles();
    statusBar()->showMessage("Saved " + path, 5000);
    return true;
}

void MainWindow::exportDocument() {
    CanvasView* view = currentView();
    if (!view) return;
    QString path = QFileDialog::getSaveFileName(this, "Export Image", view->document().name + ".png", FileIO::exportFilter());
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += ".png";
    QString err;
    if (!FileIO::exportImage(view->document(), path, 92, &err)) {
        QMessageBox::warning(this, "Export failed", err);
        return;
    }
    statusBar()->showMessage("Exported " + path, 5000);
}

bool MainWindow::confirmClose(CanvasView* view) {
    if (!view || view->undoStack()->isClean()) return true;
    auto r = QMessageBox::question(this, "Unsaved changes",
                                   QString("Save changes to %1?").arg(view->document().name),
                                   QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (r == QMessageBox::Save) return saveDocument(view, false);
    return r == QMessageBox::Discard;
}

void MainWindow::closeTab(int index) {
    auto* view = qobject_cast<CanvasView*>(m_tabs->widget(index));
    if (!view) return;
    if (!confirmClose(view)) return;
    m_tabs->removeTab(index);
    view->deleteLater();
    if (m_tabs->count() == 0) {
        m_layers->setView(nullptr);
        m_props->setView(nullptr);
        m_history->setView(nullptr);
    }
    updateUi();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    while (m_tabs->count() > 0) {
        auto* v = qobject_cast<CanvasView*>(m_tabs->widget(0));
        if (!confirmClose(v)) {
            e->ignore();
            return;
        }
        m_tabs->removeTab(0);
        v->deleteLater();
    }
    e->accept();
}

void MainWindow::updateTabTitles() {
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* v = qobject_cast<CanvasView*>(m_tabs->widget(i));
        if (!v) continue;
        const bool dirty = !v->undoStack()->isClean();
        m_tabs->setTabText(i, v->document().name + (dirty ? " *" : ""));
    }
    setWindowTitle(QString("NexImage Editor"));
}

void MainWindow::updateUi() {
    CanvasView* v = currentView();
    m_undoAct->setEnabled(v && v->undoStack()->canUndo());
    m_redoAct->setEnabled(v && v->undoStack()->canRedo());
    if (v) {
        m_undoAct->setText(v->undoStack()->canUndo() ? "Undo " + v->undoStack()->undoText() : "Undo");
        m_redoAct->setText(v->undoStack()->canRedo() ? "Redo " + v->undoStack()->redoText() : "Redo");
        const Document& d = v->document();
        m_sizeLabel->setText(QString("Doc: %1 × %2 px").arg(d.width()).arg(d.height()));
        m_zoomLabel->setText(QString("%1%").arg(v->zoom() * 100, 0, 'f', v->zoom() < 1 ? 1 : 0));
        if (QAction* a = m_toolActions.value(v->toolId())) a->setChecked(true);
    } else {
        m_sizeLabel->clear();
        m_zoomLabel->clear();
        m_posLabel->clear();
    }
    m_saveAct->setEnabled(v != nullptr);
    m_closeAct->setEnabled(v != nullptr);
    updateTabTitles();
}

// ---------------------------------------------------------------- Edit

void MainWindow::copySelection() {
    CanvasView* v = currentView();
    if (!v) return;
    const Document& d = v->document();
    const Layer* l = d.active();
    if (!l) return;
    QRect bounds = d.selection ? d.selection->bounds() : QRect(0, 0, d.width(), d.height());
    if (bounds.isEmpty()) return;
    QImage out(bounds.size(), kPremult);
    out.fill(Qt::transparent);
    QPainter p(&out);
    if (d.selection) p.setClipPath(d.selection->clip);  // canvas coordinates, set before translating
    p.translate(-bounds.topLeft());
    p.drawImage(l->x, l->y, l->image);
    p.end();
    QApplication::clipboard()->setImage(out);
    statusBar()->showMessage("Copied selection", 3000);
}

void MainWindow::pasteImage() {
    CanvasView* v = currentView();
    if (!v) return;
    QImage img = QApplication::clipboard()->image();
    if (img.isNull()) return;
    const int x = (v->document().width() - img.width()) / 2;
    const int y = (v->document().height() - img.height()) / 2;
    v->editDocument("Paste", [img, x, y](Document& d) {
        d.addLayer("Pasted Layer", img.convertToFormat(kPremult), x, y);
    });
}

void MainWindow::selectAll() {
    if (CanvasView* v = currentView())
        v->editDocument("Select All", [](Document& d) {
            d.selection = Selection::fromMask(Ops::rectMask(QRect(0, 0, d.width(), d.height()), d.width(), d.height()),
                                              d.width(), d.height());
        });
}

void MainWindow::deselect() {
    if (CanvasView* v = currentView())
        v->editDocument("Deselect", [](Document& d) { d.selection.reset(); });
}

void MainWindow::invertSelection() {
    if (CanvasView* v = currentView())
        v->editDocument("Invert Selection", [](Document& d) {
            std::vector<uint8_t> m(size_t(d.width()) * size_t(d.height()), 255);
            if (d.selection)
                for (size_t i = 0; i < m.size(); ++i) m[i] = uint8_t(255 - d.selection->mask[i]);
            d.selection = Selection::fromMask(std::move(m), d.width(), d.height());
        });
}

void MainWindow::fillSelection() {
    CanvasView* v = currentView();
    if (!v) return;
    const int idx = v->document().activeIndex();
    const QColor color = m_opts.fg;
    v->editLayer(idx, "Fill", [this, color](Layer& l) {
        if (l.locked) return;
        CanvasView* cv = currentView();
        auto mask = Ops::layerMaskFromSelection(l, cv ? cv->document().selection.get() : nullptr);
        Ops::fillWithMask(l, mask, color);
    });
}

// ---------------------------------------------------------------- Image / layer

void MainWindow::imageSize() {
    CanvasView* v = currentView();
    if (!v) return;
    const Document& d = v->document();
    SizeDialog dlg(this, "Image Size", d.width(), d.height(), false);
    if (dlg.exec() != QDialog::Accepted) return;
    const int w = dlg.width(), h = dlg.height();
    v->editDocument("Image Size", [w, h](Document& doc) { Ops::resizeImage(doc, w, h); });
}

void MainWindow::canvasSize() {
    CanvasView* v = currentView();
    if (!v) return;
    const Document& d = v->document();
    SizeDialog dlg(this, "Canvas Size", d.width(), d.height(), true);
    if (dlg.exec() != QDialog::Accepted) return;
    const int w = dlg.width(), h = dlg.height(), ax = dlg.anchorX(), ay = dlg.anchorY();
    v->editDocument("Canvas Size", [=](Document& doc) { Ops::resizeCanvas(doc, w, h, ax, ay); });
}

void MainWindow::rotate(int degrees) {
    if (CanvasView* v = currentView())
        v->editDocument("Rotate", [degrees](Document& d) { Ops::rotateDocument(d, degrees); });
}

void MainWindow::flip(bool horizontal) {
    if (CanvasView* v = currentView())
        v->editDocument("Flip", [horizontal](Document& d) { Ops::flipDocument(d, horizontal); });
}

// ---------------------------------------------------------------- Adjustments / filters

void MainWindow::applyAdjustment(const QString& name, const std::function<void(Pixels&)>& fn) {
    CanvasView* v = currentView();
    if (!v) return;
    const int idx = v->document().activeIndex();
    v->editLayer(idx, name, [this, &fn](Layer& l) {
        if (l.locked) return;
        CanvasView* cv = currentView();
        Ops::applyPixelFn(l, cv ? cv->document().selection.get() : nullptr, fn);
    });
}

void MainWindow::applyFilter(const FilterSpec& spec) {
    CanvasView* v = currentView();
    if (!v) return;
    const Layer* l = v->document().active();
    if (!l || l->locked) return;
    FilterDialog dlg(this, spec, l->image);
    if (dlg.exec() != QDialog::Accepted) return;
    const std::vector<int> vals = dlg.values();
    applyAdjustment(spec.title, [&spec, vals](Pixels& p) { spec.apply(p, vals); });
}

void MainWindow::adjustments(int which) {
    CanvasView* v = currentView();
    if (!v) return;
    switch (which) {
    case 0:
        applyFilter({"Brightness / Contrast",
                     {{"Brightness", -100, 100, 0}, {"Contrast", -100, 100, 0}},
                     [](Pixels& p, const std::vector<int>& a) { Adjust::brightnessContrast(p, a[0], a[1]); }});
        break;
    case 1:
        applyFilter({"Hue / Saturation",
                     {{"Hue", -180, 180, 0}, {"Saturation", -100, 100, 0}, {"Lightness", -100, 100, 0}},
                     [](Pixels& p, const std::vector<int>& a) { Adjust::hueSaturation(p, a[0], a[1], a[2]); }});
        break;
    case 2:
        applyFilter({"Levels",
                     {{"Input black", 0, 254, 0}, {"Input white", 1, 255, 255}, {"Gamma (×100)", 10, 1000, 100}},
                     [](Pixels& p, const std::vector<int>& a) { Adjust::levels(p, a[0], a[1], a[2] / 100.0); }});
        break;
    case 3: {
        const QColor c = m_opts.fg;
        applyFilter({"Tint",
                     {{"Amount", 0, 100, 50}},
                     [c](Pixels& p, const std::vector<int>& a) { Adjust::tint(p, c, a[0] / 100.0); }});
        break;
    }
    case 4:
        applyAdjustment("Invert", [](Pixels& p) { Adjust::invert(p); });
        break;
    case 5:
        applyAdjustment("Grayscale", [](Pixels& p) { Adjust::grayscale(p); });
        break;
    }
}

void MainWindow::filters(int which) {
    switch (which) {
    case 0:
        applyFilter({"Gaussian Blur", {{"Radius (×0.1 px)", 1, 300, 20}},
                     [](Pixels& p, const std::vector<int>& a) { Filters::gaussianBlur(p, a[0] / 10.0); }});
        break;
    case 1:
        applyFilter({"Unsharp Mask",
                     {{"Radius (×0.1 px)", 1, 300, 10}, {"Amount (%)", 0, 500, 100}, {"Threshold", 0, 255, 0}},
                     [](Pixels& p, const std::vector<int>& a) { Filters::unsharpMask(p, a[0] / 10.0, a[1] / 100.0, a[2]); }});
        break;
    case 2:
        applyFilter({"Median", {{"Radius (px)", 1, 10, 1}},
                     [](Pixels& p, const std::vector<int>& a) { Filters::median(p, a[0]); }});
        break;
    case 3:
        applyFilter({"Add Noise",
                     {{"Amount", 0, 100, 10}, {"Monochrome (0/1)", 0, 1, 1}},
                     [](Pixels& p, const std::vector<int>& a) { Filters::addNoise(p, a[0] / 10.0 * 4.0, a[1] != 0); }});
        break;
    }
}
