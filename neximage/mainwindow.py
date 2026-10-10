"""Main application window: menus, toolbox, options bar, tabs, panels, status bar."""
import os

from PySide6.QtCore import Qt, QSize
from PySide6.QtGui import QAction, QActionGroup, QKeySequence, QGuiApplication
from PySide6.QtWidgets import (QMainWindow, QTabWidget, QStackedWidget, QToolBar, QDockWidget,
                               QLabel, QFileDialog, QMessageBox, QInputDialog, QComboBox,
                               QStatusBar, QUndoView, QWidget, QSizePolicy)

from . import APP_NAME, PROJECT_EXT, __version__
from . import adjustments as adj
from . import fileio, ops
from . import imgutil as iu
from .canvas import CanvasView
from .dialogs import (NewDocumentDialog, ImageSizeDialog, CanvasSizeDialog, EffectDialog)
from .document import Document
from .icons import make_icon
from .panels import LayersPanel, PropertiesPanel, ColorSwatch, build_option_bar
from .settings import Settings
from .tools import TOOLS_BY_NAME

TOOLBOX_ORDER = ["move", "marquee", "lasso", "wand", None, "crop", "eyedropper", None,
                 "brush", "eraser", "bucket", "gradient", None, "text", "shape", None,
                 "hand", "zoom"]
ZOOM_LEVELS = [(0.125, "12.5%"), (0.25, "25%"), (0.5, "50%"), (0.667, "66.7%"), (1.0, "100%"),
               (1.5, "150%"), (2.0, "200%"), (3.0, "300%"), (4.0, "400%"), (8.0, "800%")]

WELCOME_TEXT = """<div style='text-align:center'>
<p style='font-size:20px;color:#cfcfcf'><b>NexImageEditor</b></p>
<p>Ctrl+N — New document &nbsp;·&nbsp; Ctrl+O — Open image or project</p>
<p>Ctrl+Shift+N — New layer &nbsp;·&nbsp; Ctrl+Z — Undo &nbsp;·&nbsp; Ctrl+S — Save project (.nxe)</p>
</div>"""


class MainWindow(QMainWindow):
    def __init__(self, files=()):
        super().__init__()
        self.settings = Settings()
        self._need_doc: list[QAction] = []
        self._untitled = 0
        self.setWindowTitle(APP_NAME)
        self.resize(1480, 900)
        self.setDockNestingEnabled(True)

        # central area: welcome screen or tabbed documents
        self.tabs = QTabWidget()
        self.tabs.setTabsClosable(True)
        self.tabs.setMovable(True)
        self.tabs.setDocumentMode(True)
        self.tabs.tabCloseRequested.connect(self.close_tab)
        self.tabs.currentChanged.connect(self._on_tab_changed)
        self.welcome = QLabel(WELCOME_TEXT)
        self.welcome.setObjectName("Welcome")
        self.welcome.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.central = QStackedWidget()
        self.central.addWidget(self.welcome)
        self.central.addWidget(self.tabs)
        self.setCentralWidget(self.central)

        self._create_actions()
        self._create_toolbox()
        self._create_options_bar()
        self._create_docks()
        self._create_menus()
        self._create_statusbar()

        self.settings.toolChanged.connect(self._sync_tool_actions)
        self._sync_tool_actions(self.settings.tool)
        self._update_state()
        for path in files:
            self.open_path(path)

    # ================================================================ state
    def current_view(self) -> CanvasView | None:
        w = self.tabs.currentWidget()
        return w if isinstance(w, CanvasView) else None

    def current_doc(self) -> Document | None:
        v = self.current_view()
        return v.doc if v else None

    def _update_state(self):
        view = self.current_view()
        doc = view.doc if view else None
        self.central.setCurrentWidget(self.tabs if doc else self.welcome)
        for a in self._need_doc:
            a.setEnabled(doc is not None)
        if doc is not None:
            stack = doc.undo_stack
            self.undo_action.setEnabled(stack.canUndo())
            self.redo_action.setEnabled(stack.canRedo())
            self.undo_action.setText(f"Undo {stack.undoText()}".strip())
            self.redo_action.setText(f"Redo {stack.redoText()}".strip())
            self.layers_panel.set_document(doc)
            self.props_panel.set_document(doc)
            self.history_view.setStack(stack)
            self.layer_props_action.setEnabled(doc.active_layer is not None)
            name = doc.name + ("" if stack.isClean() else " •")
            self.setWindowTitle(f"{name} — {APP_NAME}")
            self.zoom_combo_sync(view.zoom)
        else:
            self.layers_panel.set_document(None)
            self.props_panel.set_document(None)
            self.history_view.setStack(None)
            self.setWindowTitle(APP_NAME)
            self.zoom_combo.setEnabled(False)
            self.doc_info.setText("")
            self.cursor_info.setText("")
        self.zoom_combo.setEnabled(doc is not None)

    def zoom_combo_sync(self, zoom: float):
        self.zoom_combo.blockSignals(True)
        self.zoom_combo.setCurrentText(f"{round(zoom * 100, 1):g}%")
        self.zoom_combo.blockSignals(False)

    def _sync_tool_actions(self, name: str):
        if name in self.tool_actions:
            self.tool_actions[name].setChecked(True)
        page = self.option_pages.get(name)
        if page is not None:
            self.options_stack.setCurrentWidget(page)

    # ============================================================ actions
    def _act(self, text, slot, shortcut=None, need_doc=True, checkable=False, tip=None):
        a = QAction(text, self)
        if shortcut:
            a.setShortcut(QKeySequence(shortcut))
        a.setShortcutContext(Qt.ShortcutContext.WindowShortcut)
        a.setCheckable(checkable)
        if tip:
            a.setStatusTip(tip)
        a.triggered.connect(slot)
        self.addAction(a)
        if need_doc:
            self._need_doc.append(a)
        return a

    def _create_actions(self):
        A = self._act
        self.new_action = A("New…", self.new_document, "Ctrl+N", need_doc=False)
        self.open_action = A("Open…", self.open_file, "Ctrl+O", need_doc=False)
        self.place_action = A("Place Image as Layer…", self.place_image, "Ctrl+Shift+O")
        self.save_action = A("Save", self.save_file, "Ctrl+S")
        self.save_as_action = A("Save As…", self.save_file_as, "Ctrl+Shift+S")
        self.export_action = A("Export As…", self.export_file, "Ctrl+Alt+E")
        self.close_action = A("Close", lambda: self.close_tab(self.tabs.currentIndex()), "Ctrl+W")
        self.quit_action = A("Quit", self.close, "Ctrl+Q", need_doc=False)

        self.undo_action = A("Undo", self.undo, "Ctrl+Z")
        self.redo_action = A("Redo", self.redo, "Ctrl+Shift+Z")
        self.cut_action = A("Cut", self.cut, "Ctrl+X")
        self.copy_action = A("Copy", lambda: self.copy(False), "Ctrl+C")
        self.copy_merged_action = A("Copy Merged", lambda: self.copy(True), "Ctrl+Shift+C")
        self.paste_action = A("Paste as New Layer", self.paste, "Ctrl+V")
        self.clear_action = A("Clear", self.clear_selection_pixels, "Delete")
        self.fill_action = A("Fill with Foreground Color", self.fill_fg, "Alt+Backspace")
        self.transform_action = A("Free Transform", self.free_transform, "Ctrl+T")
        self.flip_h_layer = A("Flip Layer Horizontal", lambda: self._do(ops.flip_layer, True))
        self.flip_v_layer = A("Flip Layer Vertical", lambda: self._do(ops.flip_layer, False))
        self.swap_colors = A("Swap Colors", self.settings.swap_colors, "X", need_doc=False)
        self.reset_colors = A("Default Colors", self.settings.reset_colors, "D", need_doc=False)

        self.bc_action = A("Brightness / Contrast…", self.adj_brightness_contrast)
        self.hs_action = A("Hue / Saturation…", self.adj_hue_saturation, "Ctrl+U")
        self.levels_action = A("Levels…", self.adj_levels, "Ctrl+L")
        self.threshold_action = A("Threshold…", self.adj_threshold)
        self.desat_action = A("Desaturate", lambda: self._apply_pixels(adj.desaturate, "Desaturate"),
                              "Ctrl+Shift+U")
        self.invert_action = A("Invert", lambda: self._apply_pixels(adj.invert, "Invert"), "Ctrl+I")
        self.image_size_action = A("Image Size…", self.image_size, "Ctrl+Alt+I")
        self.canvas_size_action = A("Canvas Size…", self.canvas_size, "Ctrl+Alt+C")
        self.rot_cw = A("Rotate 90° Clockwise", lambda: self._do(ops.rotate_canvas, 90))
        self.rot_ccw = A("Rotate 90° Counter-Clockwise", lambda: self._do(ops.rotate_canvas, -90))
        self.rot_180 = A("Rotate 180°", lambda: self._do(ops.rotate_canvas, 180))
        self.flip_h = A("Flip Image Horizontal", lambda: self._do(ops.flip_canvas, True))
        self.flip_v = A("Flip Image Vertical", lambda: self._do(ops.flip_canvas, False))

        self.new_layer_action = A("New Layer", lambda: self._do(ops.add_layer), "Ctrl+Shift+N")
        self.layer_via_copy_action = A("New Layer via Copy", lambda: self._do(ops.layer_via_copy), "Ctrl+J")
        self.dup_layer_action = A("Duplicate Layer", lambda: self._do(ops.duplicate_layer))
        self.del_layer_action = A("Delete Layer", lambda: self._do(ops.delete_layer))
        self.layer_up_action = A("Move Layer Up", lambda: self._do(ops.move_layer, 1), "Ctrl+]")
        self.layer_down_action = A("Move Layer Down", lambda: self._do(ops.move_layer, -1), "Ctrl+[")
        self.merge_action = A("Merge Down", lambda: self._do(ops.merge_down), "Ctrl+E")
        self.flatten_action = A("Flatten Image", lambda: self._do(ops.flatten), "Ctrl+Shift+E")
        self.layer_props_action = A("Rename Layer…", self.rename_layer)

        self.select_all_action = A("All", lambda: self._do(ops.select_all), "Ctrl+A")
        self.deselect_action = A("Deselect", lambda: self._do(ops.deselect), "Ctrl+D")
        self.inverse_action = A("Inverse", lambda: self._do(ops.invert_selection), "Ctrl+Shift+I")

        self.blur_action = A("Gaussian Blur…", lambda: self.filter_dialog("blur"))
        self.unsharp_action = A("Unsharp Mask…", lambda: self.filter_dialog("unsharp"))
        self.median_action = A("Median…", lambda: self.filter_dialog("median"))
        self.noise_action = A("Add Noise…", lambda: self.filter_dialog("noise"))

        self.zoom_in_action = A("Zoom In", lambda: self._zoom_step(1.25), "Ctrl+=")
        self.zoom_out_action = A("Zoom Out", lambda: self._zoom_step(1 / 1.25), "Ctrl+-")
        self.zoom_100_action = A("Actual Pixels", lambda: self._zoom_to(1.0), "Ctrl+1")
        self.zoom_fit_action = A("Fit on Screen", lambda: self._zoom_fit(), "Ctrl+0")
        self.about_action = A("About NexImageEditor", self.about, need_doc=False)
        self.shortcuts_action = A("Keyboard Shortcuts", self.show_shortcuts, need_doc=False)

        # Tool actions: exclusive group, shortcuts from each tool class.
        self.tool_actions: dict[str, QAction] = {}
        self.tool_group = QActionGroup(self)
        self.tool_group.setExclusive(True)
        for name in TOOLS_BY_NAME:
            cls = TOOLS_BY_NAME[name]
            if name == "transform":
                continue
            a = QAction(make_icon(cls.icon), cls.label, self)
            a.setCheckable(True)
            if cls.shortcut:
                a.setShortcut(QKeySequence(cls.shortcut))
                a.setShortcutContext(Qt.ShortcutContext.WindowShortcut)
            a.setToolTip(f"{cls.label} ({cls.shortcut})" if cls.shortcut else cls.label)
            a.triggered.connect(lambda _c=False, n=name: self.select_tool(n))
            self.tool_group.addAction(a)
            self.addAction(a)
            self.tool_actions[name] = a

    def _do(self, fn, *args):
        doc = self.current_doc()
        if doc is None:
            return
        fn(doc, *args)
        view = self.current_view()
        if fn in (ops.crop_canvas, ops.resize_canvas, ops.resample_image, ops.rotate_canvas,
                  ops.flip_canvas):
            view.fit()
        self._update_state()

    def select_tool(self, name: str):
        view = self.current_view()
        if view is not None:
            view.set_tool(name)
        else:
            self.settings.tool = name

    # ----------------------------------------------------------- toolbox
    def _create_toolbox(self):
        self.toolbox = QToolBar("Tools")
        self.toolbox.setObjectName("Toolbox")
        self.toolbox.setOrientation(Qt.Orientation.Vertical)
        self.toolbox.setMovable(False)
        self.toolbox.setIconSize(QSize(24, 24))
        self.addToolBar(Qt.ToolBarArea.LeftToolBarArea, self.toolbox)
        for name in TOOLBOX_ORDER:
            if name is None:
                self.toolbox.addSeparator()
            else:
                self.toolbox.addAction(self.tool_actions[name])
        spacer = QWidget()
        spacer.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.toolbox.addWidget(spacer)
        self.toolbox.addWidget(ColorSwatch(self.settings))

    def _create_options_bar(self):
        self.options_bar = QToolBar("Tool Options")
        self.options_bar.setObjectName("OptionsBar")
        self.options_bar.setMovable(False)
        self.addToolBar(Qt.ToolBarArea.TopToolBarArea, self.options_bar)
        self.options_stack = QStackedWidget()
        self.options_bar.addWidget(self.options_stack)
        built = build_option_bar(self.settings)
        self.option_pages: dict[str, QWidget] = {}
        for name, widget in built.items():
            self.options_stack.addWidget(widget)
            self.option_pages[name] = widget
        self.options_stack.setCurrentWidget(self.option_pages["move"])

    # -------------------------------------------------------------- docks
    def _create_docks(self):
        self.layers_panel = LayersPanel()
        self.layers_dock = QDockWidget("Layers", self)
        self.layers_dock.setObjectName("LayersDock")
        self.layers_dock.setWidget(self.layers_panel)
        self.history_view = QUndoView()
        self.history_dock = QDockWidget("History", self)
        self.history_dock.setObjectName("HistoryDock")
        self.history_dock.setWidget(self.history_view)
        self.props_panel = PropertiesPanel()
        self.props_dock = QDockWidget("Properties", self)
        self.props_dock.setObjectName("PropertiesDock")
        self.props_dock.setWidget(self.props_panel)
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, self.layers_dock)
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, self.history_dock)
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, self.props_dock)
        self.tabifyDockWidget(self.layers_dock, self.history_dock)
        self.tabifyDockWidget(self.history_dock, self.props_dock)
        self.layers_dock.raise_()
        self.layers_dock.setMinimumWidth(270)

    def _create_menus(self):
        mb = self.menuBar()
        m = mb.addMenu("&File")
        for a in [self.new_action, self.open_action, self.place_action, None, self.save_action,
                  self.save_as_action, self.export_action, None, self.close_action,
                  self.quit_action]:
            m.addSeparator() if a is None else m.addAction(a)

        m = mb.addMenu("&Edit")
        for a in [self.undo_action, self.redo_action, None, self.cut_action, self.copy_action,
                  self.copy_merged_action, self.paste_action, self.clear_action,
                  self.fill_action, None, self.transform_action, self.flip_h_layer,
                  self.flip_v_layer, None, self.swap_colors, self.reset_colors]:
            m.addSeparator() if a is None else m.addAction(a)

        m = mb.addMenu("&Image")
        adjm = m.addMenu("Adjustments")
        for a in [self.bc_action, self.hs_action, self.levels_action, self.threshold_action,
                  None, self.desat_action, self.invert_action]:
            adjm.addSeparator() if a is None else adjm.addAction(a)
        for a in [None, self.image_size_action, self.canvas_size_action, None, self.rot_cw,
                  self.rot_ccw, self.rot_180, self.flip_h, self.flip_v]:
            m.addSeparator() if a is None else m.addAction(a)

        m = mb.addMenu("&Layer")
        for a in [self.new_layer_action, self.layer_via_copy_action, self.dup_layer_action,
                  self.del_layer_action, None, self.layer_up_action, self.layer_down_action,
                  None, self.merge_action, self.flatten_action, None, self.layer_props_action]:
            m.addSeparator() if a is None else m.addAction(a)

        m = mb.addMenu("&Select")
        for a in [self.select_all_action, self.deselect_action, self.inverse_action]:
            m.addAction(a)

        m = mb.addMenu("&Filter")
        for a in [self.blur_action, self.unsharp_action, self.median_action, None, self.noise_action]:
            m.addSeparator() if a is None else m.addAction(a)

        m = mb.addMenu("&View")
        for a in [self.zoom_in_action, self.zoom_out_action, self.zoom_100_action,
                  self.zoom_fit_action, None]:
            m.addSeparator() if a is None else m.addAction(a)
        for dock in (self.layers_dock, self.history_dock, self.props_dock, self.toolbox,
                     self.options_bar):
            m.addAction(dock.toggleViewAction())

        m = mb.addMenu("&Help")
        m.addAction(self.shortcuts_action)
        m.addAction(self.about_action)


    def _create_statusbar(self):
        sb = QStatusBar()
        self.setStatusBar(sb)
        self.zoom_combo = QComboBox()
        self.zoom_combo.setMinimumWidth(90)
        for _, label in ZOOM_LEVELS:
            self.zoom_combo.addItem(label)
        self.zoom_combo.addItem("Fit")
        self.zoom_combo.activated.connect(self._zoom_combo_activated)
        sb.addPermanentWidget(QLabel("Zoom:"))
        sb.addPermanentWidget(self.zoom_combo)
        self.doc_info = QLabel("")
        self.cursor_info = QLabel("")
        self.cursor_info.setMinimumWidth(170)
        sb.addPermanentWidget(self.doc_info)
        sb.addPermanentWidget(self.cursor_info)
        self.hint = QLabel("Ready")
        sb.addWidget(self.hint, 1)

    # ============================================================ tabs/docs
    def add_document(self, doc: Document):
        view = CanvasView(doc, self.settings)
        view.zoomChanged.connect(lambda z, v=view: self._on_zoom(v, z))
        view.cursorChanged.connect(lambda p, v=view: self._on_cursor(v, p))
        view.messageRequested.connect(lambda t, v=view: self._on_message(v, t))
        view.toolChanged.connect(lambda n, v=view: self._on_view_tool(v, n))
        doc.undo_stack.cleanChanged.connect(lambda _c: self._refresh_tab_titles())
        doc.undo_stack.indexChanged.connect(lambda _i: self._update_state())
        idx = self.tabs.addTab(view, doc.name)
        self.tabs.setCurrentIndex(idx)
        view.set_tool(self.settings.tool)
        self._refresh_tab_titles()
        self._update_state()
        view.setFocus()
        return view

    def _refresh_tab_titles(self):
        for i in range(self.tabs.count()):
            v = self.tabs.widget(i)
            if isinstance(v, CanvasView):
                dirty = not v.doc.undo_stack.isClean()
                self.tabs.setTabText(i, v.doc.name + (" •" if dirty else ""))
        self._update_state()

    def _on_tab_changed(self, _idx):
        view = self.current_view()
        if view is not None:
            if view.tool.name != self.settings.tool:
                view.set_tool(self.settings.tool)
            self.doc_info.setText(f"{view.doc.width} × {view.doc.height} px")
            self.zoom_combo_sync(view.zoom)
            view.setFocus()
        self._update_state()

    def _on_view_tool(self, view, name):
        if view is self.current_view() and name in self.tool_actions:
            self._sync_tool_actions(name)

    def _on_zoom(self, view, z):
        if view is self.current_view():
            self.zoom_combo_sync(z)

    def _on_cursor(self, view, p):
        if view is not self.current_view():
            return
        if p is None:
            self.cursor_info.setText("")
        else:
            x, y = int(p.x()), int(p.y())
            inside = 0 <= x < view.doc.width and 0 <= y < view.doc.height
            self.cursor_info.setText(f"X: {x}  Y: {y}" if inside else "")

    def _on_message(self, view, text):
        if view is self.current_view():
            self.hint.setText(text)

    def close_tab(self, index: int) -> bool:
        view = self.tabs.widget(index)
        if not isinstance(view, CanvasView):
            return True
        if not view.doc.undo_stack.isClean():
            self.tabs.setCurrentIndex(index)
            box = QMessageBox(self)
            box.setWindowTitle(APP_NAME)
            box.setText(f"Save changes to “{view.doc.name}” before closing?")
            save = box.addButton("Save", QMessageBox.ButtonRole.AcceptRole)
            discard = box.addButton("Don't Save", QMessageBox.ButtonRole.DestructiveRole)
            box.addButton("Cancel", QMessageBox.ButtonRole.RejectRole)
            box.exec()
            clicked = box.clickedButton()
            if clicked is save:
                if not self.save_file():
                    return False
            elif clicked is not discard:
                return False
        self.tabs.removeTab(index)
        view.deleteLater()
        self._update_state()
        return True

    # ============================================================ file
    def new_document(self):
        dlg = NewDocumentDialog(self)
        if dlg.exec() != dlg.DialogCode.Accepted:
            return
        w, h, fill = dlg.result_values()
        self._untitled += 1
        doc = Document(w, h, name=f"Untitled-{self._untitled}", background=fill)
        self.add_document(doc)

    def open_file(self):
        paths, _ = QFileDialog.getOpenFileNames(
            self, "Open", "", f"All supported (*{PROJECT_EXT} *.png *.jpg *.jpeg *.webp *.bmp "
                              f"*.tif *.tiff *.gif);;NexImageEditor project (*{PROJECT_EXT});;"
                              f"{fileio.IMAGE_FILTER}")
        for p in paths:
            self.open_path(p)

    def open_path(self, path: str):
        try:
            if path.lower().endswith(PROJECT_EXT):
                doc = fileio.load_project(path)
            else:
                doc = fileio.open_image_document(path)
                doc.file_path = None
        except Exception as exc:  # noqa: BLE001 - show any read error to the user
            QMessageBox.warning(self, APP_NAME, f"Could not open {path}:\n{exc}")
            return
        self.add_document(doc)
        self.hint.setText(f"Opened {os.path.basename(path)}")

    def place_image(self):
        doc = self.current_doc()
        if doc is None:
            return
        path, _ = QFileDialog.getOpenFileName(self, "Place Image as Layer", "", fileio.IMAGE_FILTER)
        if not path:
            return
        try:
            img = fileio.read_image(path)
        except ValueError as exc:
            QMessageBox.warning(self, APP_NAME, str(exc))
            return
        ops.paste_as_layer(doc, img, center=True, name=os.path.splitext(os.path.basename(path))[0])
        self._update_state()

    def save_file(self) -> bool:
        doc = self.current_doc()
        if doc is None:
            return False
        if not doc.file_path:
            return self.save_file_as()
        return self._write_project(doc, doc.file_path)

    def save_file_as(self) -> bool:
        doc = self.current_doc()
        if doc is None:
            return False
        path, _ = QFileDialog.getSaveFileName(self, "Save Project", doc.name + PROJECT_EXT,
                                              f"NexImageEditor project (*{PROJECT_EXT})")
        if not path:
            return False
        if not path.lower().endswith(PROJECT_EXT):
            path += PROJECT_EXT
        return self._write_project(doc, path)

    def _write_project(self, doc: Document, path: str) -> bool:
        try:
            fileio.save_project(doc, path)
        except OSError as exc:
            QMessageBox.warning(self, APP_NAME, f"Could not save:\n{exc}")
            return False
        doc.file_path = path
        doc.name = os.path.basename(path)
        doc.undo_stack.setClean()
        self.hint.setText(f"Saved {path}")
        self._refresh_tab_titles()
        return True

    def export_file(self):
        doc = self.current_doc()
        if doc is None:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "Export As", os.path.splitext(doc.name)[0] + ".png",
            "PNG (*.png);;JPEG (*.jpg *.jpeg);;WebP (*.webp);;BMP (*.bmp);;TIFF (*.tif *.tiff)")
        if not path:
            return
        if os.path.splitext(path)[1] == "":
            path += ".png"
        try:
            fileio.export_image(doc, path)
            self.hint.setText(f"Exported {path}")
        except (OSError, ValueError) as exc:
            QMessageBox.warning(self, APP_NAME, f"Export failed:\n{exc}")

    def closeEvent(self, event):
        for i in range(self.tabs.count() - 1, -1, -1):
            self.tabs.setCurrentIndex(i)
            if not self.close_tab(i):
                event.ignore()
                return
        event.accept()

    # ============================================================ edit
    def undo(self):
        doc = self.current_doc()
        if doc:
            doc.undo_stack.undo()

    def redo(self):
        doc = self.current_doc()
        if doc:
            doc.undo_stack.redo()

    def copy(self, merged: bool):
        doc = self.current_doc()
        if doc is None:
            return
        img = ops.selection_image(doc, merged=merged)
        if img is None:
            self.hint.setText("Nothing to copy")
            return
        QGuiApplication.clipboard().setImage(img)
        self.hint.setText("Copied to clipboard")

    def cut(self):
        self.copy(False)
        self.clear_selection_pixels()

    def paste(self):
        doc = self.current_doc()
        if doc is None:
            return
        img = QGuiApplication.clipboard().image()
        if img.isNull():
            self.hint.setText("Clipboard has no image")
            return
        ops.paste_as_layer(doc, img, center=True)
        self._update_state()

    def clear_selection_pixels(self):
        doc = self.current_doc()
        if doc is not None:
            ops.clear_selection(doc)
            self._update_state()

    def fill_fg(self):
        doc = self.current_doc()
        if doc is not None:
            ops.fill_selection(doc, self.settings.fg, "Fill")
            self._update_state()

    def free_transform(self):
        view = self.current_view()
        if view is not None:
            view.start_transform()

    def rename_layer(self):
        doc = self.current_doc()
        layer = doc.active_layer if doc else None
        if layer is None:
            return
        name, ok = QInputDialog.getText(self, "Layer Name", "Name:", text=layer.name)
        if ok and name.strip():
            ops.set_layer_props(doc, layer, "Rename Layer", name=name.strip())

    # ========================================================= adjustments
    def _apply_pixels(self, fn, text):
        doc = self.current_doc()
        if doc is None:
            return
        ops.apply_pixels(doc, fn, text)
        self._update_state()

    def _effect(self, title, undo_text, specs, fn):
        doc = self.current_doc()
        if doc is None or doc.active_layer is None:
            return
        dlg = EffectDialog(doc, title, undo_text, specs, fn, self)
        dlg.exec()
        self._update_state()

    def adj_brightness_contrast(self):
        self._effect("Brightness / Contrast", "Brightness/Contrast", [
            ("brightness", "Brightness", "int", -100, 100, 0, 0),
            ("contrast", "Contrast", "int", -100, 100, 0, 0),
        ], adj.brightness_contrast)

    def adj_hue_saturation(self):
        self._effect("Hue / Saturation", "Hue/Saturation", [
            ("hue", "Hue", "int", -180, 180, 0, 0),
            ("saturation", "Saturation", "int", -100, 100, 0, 0),
            ("lightness", "Lightness", "int", -100, 100, 0, 0),
        ], adj.hue_saturation)

    def adj_levels(self):
        self._effect("Levels", "Levels", [
            ("black", "Input black", "int", 0, 254, 0, 0),
            ("white", "Input white", "int", 1, 255, 255, 0),
            ("gamma", "Midtone gamma", "float", 0.10, 9.99, 1.0, 2),
        ], adj.levels)

    def adj_threshold(self):
        self._effect("Threshold", "Threshold", [
            ("level", "Threshold level", "int", 0, 255, 128, 0),
        ], adj.threshold)

    def filter_dialog(self, kind: str):
        if kind == "blur":
            self._effect("Gaussian Blur", "Gaussian Blur", [
                ("radius", "Radius (px)", "float", 0.1, 100.0, 2.0, 1)], adj.gaussian_blur)
        elif kind == "unsharp":
            self._effect("Unsharp Mask", "Unsharp Mask", [
                ("amount", "Amount (%)", "int", 1, 500, 80, 0),
                ("radius", "Radius (px)", "float", 0.1, 50.0, 2.0, 1),
                ("threshold", "Threshold", "int", 0, 255, 2, 0),
            ], adj.unsharp_mask)
        elif kind == "median":
            self._effect("Median", "Median", [
                ("radius", "Radius (px)", "int", 1, 20, 1, 0)], adj.median_blur)
        elif kind == "noise":
            self._effect("Add Noise", "Add Noise", [
                ("amount", "Amount", "int", 0, 100, 20, 0),
                ("monochrome", "Monochromatic", "bool", 0, 1, False, 0),
            ], lambda rgba, amount, monochrome: adj.add_noise(rgba, amount, monochrome, seed=7))

    def image_size(self):
        doc = self.current_doc()
        if doc is None:
            return
        dlg = ImageSizeDialog(doc, self)
        if dlg.exec() == dlg.DialogCode.Accepted:
            ops.resample_image(doc, dlg.w.value(), dlg.h.value())
            self.current_view().fit()
            self._update_state()

    def canvas_size(self):
        doc = self.current_doc()
        if doc is None:
            return
        dlg = CanvasSizeDialog(doc, self)
        if dlg.exec() == dlg.DialogCode.Accepted:
            ops.resize_canvas(doc, dlg.w.value(), dlg.h.value(), dlg.anchor.currentText())
            self.current_view().fit()
            self._update_state()

    # ============================================================ view
    def _zoom_step(self, factor):
        view = self.current_view()
        if view:
            view.set_zoom(view.zoom * factor)

    def _zoom_to(self, z):
        view = self.current_view()
        if view:
            view.set_zoom(z)

    def _zoom_fit(self):
        view = self.current_view()
        if view:
            view.fit()

    def _zoom_combo_activated(self, idx):
        view = self.current_view()
        if view is None:
            return
        if idx < len(ZOOM_LEVELS):
            view.set_zoom(ZOOM_LEVELS[idx][0])
        else:
            view.fit()

    # ============================================================ help
    def about(self):
        QMessageBox.about(self, f"About {APP_NAME}",
                          f"<b>{APP_NAME}</b> {__version__}<br>"
                          "A Photoshop-style raster editor built with Qt 6 (PySide6), "
                          "NumPy and OpenCV.<br><br>"
                          "Layers · blend modes · selections · brush/eraser · fill & gradient · "
                          "crop · free transform · adjustments · filters · undo/redo · "
                          "project files (.nxe).")

    def show_shortcuts(self):
        rows = [
            ("V", "Move"), ("M", "Marquee"), ("L", "Lasso"), ("W", "Magic wand"), ("C", "Crop"),
            ("I", "Eyedropper"), ("B / E", "Brush / Eraser"), ("G / Shift+G", "Bucket / Gradient"),
            ("T", "Text"), ("U", "Shape"), ("H / Z", "Hand / Zoom"), ("Ctrl+T", "Free transform"),
            ("Ctrl+Z / Ctrl+Shift+Z", "Undo / Redo"), ("Ctrl+J", "Layer via copy"),
            ("Ctrl+Shift+N", "New layer"), ("Ctrl+E", "Merge down"), ("Ctrl+A / Ctrl+D", "Select all / none"),
            ("Ctrl+L / Ctrl+U", "Levels / Hue-Saturation"), ("Ctrl+I", "Invert"),
            ("X / D", "Swap / default colors"), ("Space + drag", "Pan"), ("Enter / Esc", "Apply / cancel crop & transform"),
        ]
        html = "<table cellpadding='4'>" + "".join(
            f"<tr><td><b>{k}</b></td><td>{v}</td></tr>" for k, v in rows) + "</table>"
        QMessageBox.information(self, "Keyboard Shortcuts", html)
