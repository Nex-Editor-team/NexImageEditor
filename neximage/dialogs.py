"""Modal dialogs: new document, sizes, and adjustment/filter dialogs with live preview."""
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (QDialog, QDialogButtonBox, QVBoxLayout, QHBoxLayout, QFormLayout,
                               QSpinBox, QComboBox, QCheckBox, QLabel, QSlider, QDoubleSpinBox,
                               QWidget)

from . import imgutil as iu
from . import ops
from .ops import ANCHORS

PRESETS = [
    ("Custom", None, None),
    ("1920 × 1080 (HD)", 1920, 1080),
    ("1280 × 720 (720p)", 1280, 720),
    ("3840 × 2160 (4K)", 3840, 2160),
    ("1080 × 1080 (Square)", 1080, 1080),
    ("1080 × 1350 (Portrait)", 1080, 1350),
    ("A4 @ 300 dpi", 2480, 3508),
]


class NewDocumentDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("New Document")
        form = QFormLayout()
        self.preset = QComboBox()
        for name, _, _ in PRESETS:
            self.preset.addItem(name)
        self.preset.currentIndexChanged.connect(self._preset_changed)
        self.width_box = QSpinBox(); self.width_box.setRange(1, 20000); self.width_box.setValue(1920)
        self.height_box = QSpinBox(); self.height_box.setRange(1, 20000); self.height_box.setValue(1080)
        self.fill = QComboBox()
        self.fill.addItems(["White", "Black", "Transparent"])
        form.addRow("Preset:", self.preset)
        form.addRow("Width (px):", self.width_box)
        form.addRow("Height (px):", self.height_box)
        form.addRow("Background:", self.fill)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok |
                                   QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        lay = QVBoxLayout(self)
        lay.addLayout(form)
        lay.addWidget(buttons)

    def _preset_changed(self, idx):
        _, w, h = PRESETS[idx]
        if w:
            self.width_box.setValue(w)
            self.height_box.setValue(h)

    def result_values(self):
        fill = {0: QColor(255, 255, 255), 1: QColor(0, 0, 0), 2: None}[self.fill.currentIndex()]
        return self.width_box.value(), self.height_box.value(), fill


class ImageSizeDialog(QDialog):
    def __init__(self, doc, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Image Size")
        self._ratio = doc.width / doc.height
        self.w = QSpinBox(); self.w.setRange(1, 20000); self.w.setValue(doc.width)
        self.h = QSpinBox(); self.h.setRange(1, 20000); self.h.setValue(doc.height)
        self.keep = QCheckBox("Constrain proportions"); self.keep.setChecked(True)
        self.w.valueChanged.connect(lambda v: self._sync(self.h, round(v / self._ratio))
                                    if self.keep.isChecked() else None)
        self.h.valueChanged.connect(lambda v: self._sync(self.w, round(v * self._ratio))
                                    if self.keep.isChecked() else None)
        form = QFormLayout()
        form.addRow("Width (px):", self.w)
        form.addRow("Height (px):", self.h)
        note = QLabel("Resamples every layer (smooth). Selection is cleared.")
        note.setWordWrap(True)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok |
                                   QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        lay = QVBoxLayout(self)
        lay.addLayout(form)
        lay.addWidget(self.keep)
        lay.addWidget(note)
        lay.addWidget(buttons)

    def _sync(self, box, value):
        box.blockSignals(True)
        box.setValue(max(1, int(value)))
        box.blockSignals(False)


class CanvasSizeDialog(QDialog):
    def __init__(self, doc, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Canvas Size")
        self.w = QSpinBox(); self.w.setRange(1, 20000); self.w.setValue(doc.width)
        self.h = QSpinBox(); self.h.setRange(1, 20000); self.h.setValue(doc.height)
        self.anchor = QComboBox(); self.anchor.addItems(ANCHORS)
        self.anchor.setCurrentText("Center")
        form = QFormLayout()
        form.addRow("Width (px):", self.w)
        form.addRow("Height (px):", self.h)
        form.addRow("Anchor:", self.anchor)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok |
                                   QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        lay = QVBoxLayout(self)
        lay.addLayout(form)
        lay.addWidget(buttons)


class _ParamRow(QWidget):
    """Slider + spin box for one numeric parameter."""

    def __init__(self, label, kind, lo, hi, default, decimals, on_change):
        super().__init__()
        self.kind = kind
        self.decimals = decimals
        self.scale = 10 ** decimals
        self.on_change = on_change
        lay = QHBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        self.slider = QSlider(Qt.Orientation.Horizontal)
        self.slider.setRange(round(lo * self.scale), round(hi * self.scale))
        if kind == "float":
            self.spin = QDoubleSpinBox()
            self.spin.setDecimals(decimals)
            self.spin.setRange(lo, hi)
            self.spin.setSingleStep(10 ** -decimals)
        else:
            self.spin = QSpinBox()
            self.spin.setRange(int(lo), int(hi))
        self.spin.setValue(default)
        self.slider.setValue(round(default * self.scale))
        self.slider.valueChanged.connect(self._from_slider)
        self.spin.valueChanged.connect(self._from_spin)
        lay.addWidget(self.slider, 1)
        lay.addWidget(self.spin)

    def _from_slider(self, v):
        val = v / self.scale if self.kind == "float" else v
        self.spin.blockSignals(True)
        self.spin.setValue(val)
        self.spin.blockSignals(False)
        self.on_change()

    def _from_spin(self, v):
        self.slider.blockSignals(True)
        self.slider.setValue(round(v * self.scale))
        self.slider.blockSignals(False)
        self.on_change()

    def value(self):
        return self.spin.value()


class EffectDialog(QDialog):
    """Generic adjustment/filter dialog.

    specs: list of (key, label, kind, lo, hi, default, decimals) with kind in
    {'int', 'float', 'bool'}. `fn(rgba, **params)` returns the processed pixels.
    The layer is previewed live; Cancel restores the original pixels.
    """

    def __init__(self, doc, title, undo_text, specs, fn, parent=None):
        super().__init__(parent)
        self.setWindowTitle(title)
        self.doc = doc
        self.undo_text = undo_text
        self.fn = fn
        self.layer = doc.active_layer
        self._orig = self.layer.image if self.layer else None
        self._before = doc.capture()
        self._rows = {}
        self._checks = {}
        form = QFormLayout()
        for key, label, kind, lo, hi, default, decimals in specs:
            if kind == "bool":
                cb = QCheckBox()
                cb.setChecked(bool(default))
                cb.toggled.connect(self._schedule)
                self._checks[key] = cb
                form.addRow(label, cb)
            else:
                row = _ParamRow(label, kind, lo, hi, default, decimals, self._schedule)
                self._rows[key] = row
                form.addRow(label, row)
        self.preview = QCheckBox("Preview")
        self.preview.setChecked(True)
        self.preview.toggled.connect(self._schedule)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok |
                                   QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        lay = QVBoxLayout(self)
        lay.addLayout(form)
        lay.addWidget(self.preview)
        lay.addWidget(buttons)
        self.setMinimumWidth(380)
        self._timer = QTimer(self)
        self._timer.setSingleShot(True)
        self._timer.setInterval(120)
        self._timer.timeout.connect(self._update_preview)

    def params(self) -> dict:
        out = {k: r.value() for k, r in self._rows.items()}
        out.update({k: c.isChecked() for k, c in self._checks.items()})
        return out

    def _schedule(self, *_):
        self._timer.start()

    def _compute(self):
        old = iu.to_rgba(self._orig)
        new = self.fn(old.copy(), **self.params())
        mask = ops.selection_window(self.doc, self.layer)
        return iu.from_rgba(iu.blend_with_mask(old, new, mask))

    def _update_preview(self):
        if self.layer is None:
            return
        if self.preview.isChecked():
            self.layer.image = self._compute()
        else:
            self.layer.image = self._orig
        self.doc.touch()

    def accept(self):
        if self.layer is not None:
            self.layer.image = self._compute()
            self.doc.commit(self.undo_text, self._before)
        super().accept()

    def reject(self):
        if self.layer is not None:
            self.layer.image = self._orig
            self.doc.touch()
        super().reject()
