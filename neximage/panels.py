"""Dock panels (Layers, Properties), the color swatch and per-tool option bars."""
from PySide6.QtCore import Qt, QRect, QSize, QTimer
from PySide6.QtGui import QIcon, QPixmap, QPainter, QColor, QPen
from PySide6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QListWidget, QListWidgetItem,
                               QComboBox, QSlider, QSpinBox, QCheckBox, QToolButton, QLabel,
                               QFormLayout, QColorDialog, QFontComboBox, QLineEdit, QFrame)

from . import ops
from .document import BLEND_NAMES, Document
from .icons import make_icon
from .settings import Settings


class LayersPanel(QWidget):
    """Photoshop-style layer stack: blend/opacity/lock header, eye + thumbnail list, footer."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.doc: Document | None = None
        self._updating = False
        self._opacity_before = None
        self._timer = QTimer(self)
        self._timer.setSingleShot(True)
        self._timer.setInterval(150)
        self._timer.timeout.connect(self.refresh)

        self.blend = QComboBox()
        self.blend.addItems(BLEND_NAMES)
        self.blend.currentTextChanged.connect(self._on_blend)
        self.opacity_slider = QSlider(Qt.Orientation.Horizontal)
        self.opacity_slider.setRange(0, 100)
        self.opacity_slider.sliderPressed.connect(self._opacity_press)
        self.opacity_slider.sliderReleased.connect(self._opacity_release)
        self.opacity_slider.valueChanged.connect(self._on_opacity)
        self.opacity_spin = QSpinBox()
        self.opacity_spin.setRange(0, 100)
        self.opacity_spin.setSuffix(" %")
        self.opacity_spin.valueChanged.connect(self.opacity_slider.setValue)
        self.lock_box = QCheckBox("Lock")
        self.lock_box.toggled.connect(self._on_lock)

        head = QHBoxLayout()
        head.addWidget(self.blend, 2)
        head.addWidget(QLabel("Opacity"))
        head.addWidget(self.opacity_slider, 2)
        head.addWidget(self.opacity_spin)
        head2 = QHBoxLayout()
        head2.addWidget(self.lock_box)
        head2.addStretch(1)

        self.list = QListWidget()
        self.list.setIconSize(QSize(40, 40))
        self.list.setSpacing(1)
        self.list.setSelectionMode(QListWidget.SelectionMode.SingleSelection)
        self.list.currentRowChanged.connect(self._on_row)
        self.list.itemChanged.connect(self._on_item_changed)
        self.list.itemDoubleClicked.connect(lambda it: self.list.editItem(it))

        foot = QHBoxLayout()
        foot.setSpacing(2)
        for icon, tip, fn in [
            ("merge", "Merge Down (Ctrl+E)", lambda: self._do(ops.merge_down)),
            ("duplicate", "Duplicate Layer", lambda: self._do(ops.duplicate_layer)),
            ("up", "Move Up", lambda: self._do(lambda d: ops.move_layer(d, 1))),
            ("down", "Move Down", lambda: self._do(lambda d: ops.move_layer(d, -1))),
            ("new", "New Layer (Ctrl+Shift+N)", lambda: self._do(ops.add_layer)),
            ("trash", "Delete Layer", lambda: self._do(ops.delete_layer)),
        ]:
            b = QToolButton()
            b.setIcon(make_icon(icon))
            b.setToolTip(tip)
            b.clicked.connect(fn)
            foot.addWidget(b)
        foot.addStretch(1)

        lay = QVBoxLayout(self)
        lay.setContentsMargins(6, 6, 6, 6)
        lay.addLayout(head)
        lay.addLayout(head2)
        lay.addWidget(self.list, 1)
        lay.addLayout(foot)
        self.setEnabled(False)

    # ---- binding ----------------------------------------------------
    def set_document(self, doc: Document | None):
        if self.doc is not None:
            try:
                self.doc.changed.disconnect(self._timer.start)
            except (RuntimeError, TypeError):
                pass
        self.doc = doc
        self.setEnabled(doc is not None)
        if doc is not None:
            doc.changed.connect(self._timer.start)  # debounce: strokes emit many changes
        self.refresh()

    def _do(self, fn):
        if self.doc is None:
            return
        fn(self.doc)

    def refresh(self):
        if self.doc is None:
            self.list.clear()
            return
        self._updating = True
        self.list.clear()
        for idx in range(len(self.doc.layers) - 1, -1, -1):  # top layer first
            layer = self.doc.layers[idx]
            item = QListWidgetItem(layer.name)
            item.setData(Qt.ItemDataRole.UserRole, idx)
            item.setFlags(Qt.ItemFlag.ItemIsEnabled | Qt.ItemFlag.ItemIsSelectable |
                          Qt.ItemFlag.ItemIsUserCheckable | Qt.ItemFlag.ItemIsEditable)
            item.setCheckState(Qt.CheckState.Checked if layer.visible else Qt.CheckState.Unchecked)
            item.setIcon(self._thumb(layer))
            self.list.addItem(item)
            if idx == self.doc.active:
                self.list.setCurrentItem(item)
        layer = self.doc.active_layer
        if layer is not None:
            self.blend.setCurrentText(layer.blend)
            self.opacity_slider.setValue(round(layer.opacity * 100))
            self.opacity_spin.setValue(round(layer.opacity * 100))
            self.lock_box.setChecked(layer.locked)
        self._updating = False

    def _thumb(self, layer) -> QIcon:
        pix = QPixmap(40, 40)
        pix.fill(QColor(90, 90, 90))
        p = QPainter(pix)
        scaled = layer.image.scaled(40, 40, Qt.AspectRatioMode.KeepAspectRatio,
                                    Qt.TransformationMode.SmoothTransformation)
        p.drawImage((40 - scaled.width()) // 2, (40 - scaled.height()) // 2, scaled)
        p.setPen(QPen(QColor(20, 20, 20), 1))
        p.drawRect(QRect(0, 0, 39, 39))
        p.end()
        return QIcon(pix)

    # ---- user actions -----------------------------------------------
    def _on_row(self, row):
        if self._updating or self.doc is None or row < 0:
            return
        idx = len(self.doc.layers) - 1 - row
        if idx != self.doc.active:
            self.doc.active = idx
            self.doc.touch()
            self.refresh()

    def _current(self):
        return self.doc.active_layer if self.doc else None

    def _on_item_changed(self, item: QListWidgetItem):
        if self._updating or self.doc is None:
            return
        idx = item.data(Qt.ItemDataRole.UserRole)
        layer = self.doc.layers[idx]
        visible = item.checkState() == Qt.CheckState.Checked
        if layer.name != item.text() and item.text().strip():
            ops.set_layer_props(self.doc, layer, "Rename Layer", name=item.text().strip())
        elif layer.visible != visible:
            ops.set_layer_props(self.doc, layer, "Toggle Visibility", visible=visible)

    def _on_blend(self, text):
        layer = self._current()
        if self._updating or layer is None:
            return
        ops.set_layer_props(self.doc, layer, "Blend Mode", blend=text)

    def _on_lock(self, checked):
        layer = self._current()
        if self._updating or layer is None:
            return
        ops.set_layer_props(self.doc, layer, "Lock Layer", locked=checked)

    def _opacity_press(self):
        if self.doc is not None:
            self._opacity_before = self.doc.capture()

    def _opacity_release(self):
        if self._opacity_before is not None and self.doc is not None:
            self.doc.commit("Layer Opacity", self._opacity_before)
        self._opacity_before = None

    def _on_opacity(self, value):
        layer = self._current()
        if self._updating or layer is None:
            return
        self.opacity_spin.blockSignals(True)
        self.opacity_spin.setValue(value)
        self.opacity_spin.blockSignals(False)
        if self._opacity_before is not None:       # dragging: live preview only
            layer.opacity = value / 100.0
            self.doc.touch()
        else:                                       # keyboard / spin box: one undo step
            ops.set_layer_props(self.doc, layer, "Layer Opacity", opacity=value / 100.0)


class PropertiesPanel(QWidget):
    """Document and active-layer information with editable position."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.doc: Document | None = None
        self._updating = False
        self.doc_info = QLabel("-")
        self.doc_info.setWordWrap(True)
        self.name_label = QLabel("-")
        self.x_box = QSpinBox(); self.x_box.setRange(-20000, 20000)
        self.y_box = QSpinBox(); self.y_box.setRange(-20000, 20000)
        self.w_label = QLabel("-")
        self.h_label = QLabel("-")
        self.x_box.editingFinished.connect(self._apply_pos)
        self.y_box.editingFinished.connect(self._apply_pos)
        form = QFormLayout()
        form.addRow("Canvas:", self.doc_info)
        form.addRow("Layer:", self.name_label)
        form.addRow("X:", self.x_box)
        form.addRow("Y:", self.y_box)
        form.addRow("Layer W:", self.w_label)
        form.addRow("Layer H:", self.h_label)
        lay = QVBoxLayout(self)
        lay.setContentsMargins(8, 8, 8, 8)
        lay.addLayout(form)
        lay.addStretch(1)

    def set_document(self, doc: Document | None):
        if self.doc is not None:
            try:
                self.doc.changed.disconnect(self.refresh)
            except (RuntimeError, TypeError):
                pass
        self.doc = doc
        if doc is not None:
            doc.changed.connect(self.refresh)
        self.refresh()

    def refresh(self):
        if self.doc is None:
            self.doc_info.setText("No document")
            self.name_label.setText("-")
            return
        self._updating = True
        self.doc_info.setText(f"{self.doc.width} × {self.doc.height} px")
        layer = self.doc.active_layer
        enabled = layer is not None
        for w in (self.x_box, self.y_box):
            w.setEnabled(enabled)
        if layer is None:
            self.name_label.setText("-")
        else:
            self.name_label.setText(layer.name)
            self.x_box.setValue(layer.x)
            self.y_box.setValue(layer.y)
            self.w_label.setText(f"{layer.w} px")
            self.h_label.setText(f"{layer.h} px")
        self._updating = False

    def _apply_pos(self):
        layer = self.doc.active_layer if self.doc else None
        if self._updating or layer is None:
            return
        ops.set_layer_props(self.doc, layer, "Move Layer",
                            x=self.x_box.value(), y=self.y_box.value())


class ColorSwatch(QWidget):
    """Foreground / background color squares at the bottom of the toolbox."""

    def __init__(self, settings: Settings, parent=None):
        super().__init__(parent)
        self.settings = settings
        self.setFixedSize(46, 46)
        self.setToolTip("Click the front square for foreground, back square for background.\n"
                        "X swaps, D resets.")
        settings.colorsChanged.connect(self.update)

    def paintEvent(self, _e):
        p = QPainter(self)
        bg_rect = QRect(18, 18, 26, 26)
        fg_rect = QRect(2, 2, 26, 26)
        p.fillRect(bg_rect, self.settings.bg)
        p.setPen(QPen(QColor(20, 20, 20), 1))
        p.drawRect(bg_rect)
        p.fillRect(fg_rect, self.settings.fg)
        p.drawRect(fg_rect)
        p.end()

    def mousePressEvent(self, e):
        if e.button() != Qt.MouseButton.LeftButton:
            return
        in_fg = QRect(2, 2, 26, 26).contains(e.position().toPoint())
        which = self.settings.fg if in_fg else self.settings.bg
        color = QColorDialog.getColor(which, self, "Choose color")
        if color.isValid():
            if in_fg:
                self.settings.fg = color
            else:
                self.settings.bg = color


def build_option_bar(settings: Settings) -> dict[str, QWidget]:
    """One widget per tool with the options that tool uses."""
    def row(*widgets) -> QWidget:
        w = QWidget()
        lay = QHBoxLayout(w)
        lay.setContentsMargins(6, 2, 6, 2)
        lay.setSpacing(8)
        for item in widgets:
            if isinstance(item, str):
                lbl = QLabel(item)
                lbl.setObjectName("OptLabel")
                lay.addWidget(lbl)
            else:
                lay.addWidget(item)
        lay.addStretch(1)
        return w

    def spin(lo, hi, value, setter, suffix=""):
        s = QSpinBox()
        s.setRange(lo, hi)
        s.setValue(value)
        s.setSuffix(suffix)
        s.valueChanged.connect(setter)
        return s

    def combo(items, current, setter):
        c = QComboBox()
        c.addItems(items)
        c.setCurrentText(current)
        c.currentTextChanged.connect(setter)
        return c

    def check(text, value, setter):
        c = QCheckBox(text)
        c.setChecked(value)
        c.toggled.connect(setter)
        return c

    def text(s):
        lbl = QLabel(s)
        lbl.setObjectName("OptHint")
        return lbl

    def set_tol(v): settings.tolerance = v
    def set_contig(v): settings.contiguous = v
    def set_size(v): settings.brush_size = v
    def set_hard(v): settings.brush_hardness = v
    def set_opac(v): settings.brush_opacity = v

    font = QFontComboBox()
    font.setCurrentFont(font.currentFont())
    font.setCurrentText(settings.font_family)
    font.currentTextChanged.connect(lambda t: setattr(settings, "font_family", t))

    marquee = combo(["rect", "ellipse"], settings.marquee_shape,
                    lambda t: setattr(settings, "marquee_shape", t))
    shape = combo(["rect", "ellipse"], settings.shape_kind,
                  lambda t: setattr(settings, "shape_kind", t))
    tol = spin(0, 255, settings.tolerance, set_tol)
    contig = check("Contiguous", settings.contiguous, set_contig)

    return {
        "move": row(text("Drag to move the layer or the selected pixels. Arrow keys nudge.")),
        "marquee": row("Shape:", marquee, text("Shift = add · Alt = subtract · click = deselect")),
        "lasso": row(text("Drag freehand. Shift = add · Alt = subtract")),
        "wand": row("Tolerance:", tol, contig,
                    text("Shift = add · Alt = subtract")),
        "crop": row(text("Drag a crop area · Enter applies · Esc cancels")),
        "eyedropper": row(text("Click = foreground · Alt-click = background")),
        "brush": row("Size:", spin(1, 500, settings.brush_size, set_size, " px"),
                     "Hardness:", spin(0, 100, settings.brush_hardness, set_hard, " %"),
                     "Opacity:", spin(1, 100, settings.brush_opacity, set_opac, " %")),
        "eraser": row("Size:", spin(1, 500, settings.brush_size, set_size, " px"),
                      "Hardness:", spin(0, 100, settings.brush_hardness, set_hard, " %"),
                      "Opacity:", spin(1, 100, settings.brush_opacity, set_opac, " %")),
        "bucket": row("Tolerance:", tol, contig),
        "gradient": row(text("Drag: foreground → background")),
        "text": row("Font:", font, "Size:",
                    spin(6, 800, settings.font_size,
                         lambda v: setattr(settings, "font_size", v), " pt")),
        "shape": row("Shape:", shape, text("Drag to draw a filled shape on a new layer")),
        "hand": row(text("Drag to pan · Space+drag works in every tool")),
        "zoom": row(text("Click = zoom in · Alt-click = zoom out · drag = zoom to rectangle")),
        "transform": row(text("Drag handles to scale · drag inside to move · Enter apply · Esc cancel")),
    }
