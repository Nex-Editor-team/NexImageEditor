"""Document model: layers, selection, compositing and undo/redo.

Undo strategy: every editing operation is wrapped in `Document.edit(name)`.
Before the change we capture a lightweight snapshot (layer attributes plus
*shared* QImage pixel data, which Qt copies on write), and after the change
we push an EditCommand holding both snapshots.
"""
import copy
from contextlib import contextmanager

import numpy as np
import cv2
from PySide6.QtCore import QObject, Signal, QRect, Qt, QPointF
from PySide6.QtGui import (QImage, QPainter, QPainterPath, QColor, QUndoCommand,
                           QUndoStack, QPolygonF)

from . import imgutil as iu

# (display name, QPainter composition mode). Qt's painter implements the
# separable blend formulas, so we get correct alpha handling for free.
BLEND_MODES = [
    ("Normal", QPainter.CompositionMode.CompositionMode_SourceOver),
    ("Multiply", QPainter.CompositionMode.CompositionMode_Multiply),
    ("Screen", QPainter.CompositionMode.CompositionMode_Screen),
    ("Overlay", QPainter.CompositionMode.CompositionMode_Overlay),
    ("Darken", QPainter.CompositionMode.CompositionMode_Darken),
    ("Lighten", QPainter.CompositionMode.CompositionMode_Lighten),
    ("Color Dodge", QPainter.CompositionMode.CompositionMode_ColorDodge),
    ("Color Burn", QPainter.CompositionMode.CompositionMode_ColorBurn),
    ("Hard Light", QPainter.CompositionMode.CompositionMode_HardLight),
    ("Soft Light", QPainter.CompositionMode.CompositionMode_SoftLight),
    ("Difference", QPainter.CompositionMode.CompositionMode_Difference),
    ("Exclusion", QPainter.CompositionMode.CompositionMode_Exclusion),
]
BLEND_QT = dict(BLEND_MODES)
BLEND_NAMES = [name for name, _ in BLEND_MODES]


class Layer:
    def __init__(self, image: QImage, name: str = "Layer", x: int = 0, y: int = 0,
                 background: bool = False):
        self.image = iu.to_premult(image)
        self.name = name
        self.x = int(x)
        self.y = int(y)
        self.visible = True
        self.locked = False
        self.opacity = 1.0          # 0..1
        self.blend = "Normal"
        self.background = background

    @property
    def w(self) -> int:
        return self.image.width()

    @property
    def h(self) -> int:
        return self.image.height()

    def rect(self) -> QRect:
        return QRect(self.x, self.y, self.w, self.h)

    def copy(self) -> "Layer":
        """Cheap copy: attributes are copied, pixels are shared (copy-on-write)."""
        dup = copy.copy(self)
        dup.image = QImage(self.image)
        return dup


class Selection:
    """A selection stored as a canvas-sized uint8 mask (0..255) plus an outline path."""

    def __init__(self, mask: np.ndarray, path: QPainterPath):
        self.mask = mask  # never mutated in place
        self.path = path

    @staticmethod
    def from_path(path: QPainterPath, w: int, h: int) -> "Selection":
        img = iu.new_image(w, h)
        p = QPainter(img)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.fillPath(path, QColor(255, 255, 255, 255))
        p.end()
        mask = iu.to_rgba(img)[..., 3].copy()
        return Selection(mask, QPainterPath(path))

    @staticmethod
    def from_mask(mask: np.ndarray) -> "Selection | None":
        m = np.ascontiguousarray(mask, dtype=np.uint8).copy()
        if not m.any():
            return None
        contours, _ = cv2.findContours(m, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_SIMPLE)
        path = QPainterPath()
        path.setFillRule(Qt.FillRule.OddEvenFill)
        for c in contours:
            pts = c.reshape(-1, 2)
            if len(pts) < 3:
                continue
            poly = QPolygonF([QPointF(float(px), float(py)) for px, py in pts])
            path.addPolygon(poly)
            path.closeSubpath()
        return Selection(m, path)

    def bounds(self) -> QRect:
        ys, xs = np.nonzero(self.mask)
        if xs.size == 0:
            return QRect()
        return QRect(int(xs.min()), int(ys.min()),
                     int(xs.max() - xs.min() + 1), int(ys.max() - ys.min() + 1))


def combine_masks(base: "Selection | None", new: Selection, op: str,
                  w: int, h: int) -> "Selection | None":
    """op: 'replace' | 'add' | 'subtract'."""
    if op == "replace" or base is None:
        return new
    if op == "add":
        mask = np.maximum(base.mask, new.mask)
    else:
        mask = np.minimum(base.mask, 255 - new.mask)
    return Selection.from_mask(mask)


def render_stack(layers, width: int, height: int, origin: QPointF | None = None) -> QImage:
    """Composite `layers` (bottom -> top) onto a transparent canvas."""
    ox, oy = (0, 0) if origin is None else (int(origin.x()), int(origin.y()))
    img = iu.new_image(width, height)
    p = QPainter(img)
    for layer in layers:
        if not layer.visible or layer.opacity <= 0:
            continue
        p.setOpacity(layer.opacity)
        p.setCompositionMode(BLEND_QT.get(layer.blend, BLEND_QT["Normal"]))
        p.drawImage(layer.x - ox, layer.y - oy, layer.image)
    p.end()
    return img


class Document(QObject):
    changed = Signal()

    def __init__(self, width: int, height: int, name: str = "Untitled",
                 background: QColor | None = None, empty: bool = False):
        super().__init__()
        self.width = int(width)
        self.height = int(height)
        self.name = name
        self.file_path: str | None = None
        self.layers: list[Layer] = []
        self.active = -1
        self.selection: Selection | None = None
        self.preview_hidden: Layer | None = None  # hidden while free-transforming
        self.undo_stack = QUndoStack(self)
        self._version = 0
        self._comp: QImage | None = None
        self._comp_version = -1
        if not empty:
            if background is None:
                self.layers.append(Layer(iu.new_image(width, height), "Layer 1"))
            else:
                self.layers.append(Layer(iu.new_image(width, height, background),
                                         "Background", background=True))
            self.active = 0

    # ---- basic queries -------------------------------------------------
    @property
    def active_layer(self) -> Layer | None:
        if 0 <= self.active < len(self.layers):
            return self.layers[self.active]
        return None

    def canvas_rect(self) -> QRect:
        return QRect(0, 0, self.width, self.height)

    def new_layer_name(self, base: str = "Layer") -> str:
        names = {l.name for l in self.layers}
        n = len(self.layers) + 1
        while f"{base} {n}" in names:
            n += 1
        return f"{base} {n}"

    # ---- change notification and compositing ---------------------------
    def touch(self):
        """Mark pixels/structure as changed (invalidates the composite cache)."""
        self._version += 1
        self.changed.emit()

    def composite(self) -> QImage:
        if self._comp is None or self._comp_version != self._version:
            layers = [l for l in self.layers if l is not self.preview_hidden]
            self._comp = render_stack(layers, self.width, self.height)
            self._comp_version = self._version
        return self._comp

    # ---- snapshots and undo --------------------------------------------
    def capture(self) -> dict:
        return {
            "w": self.width,
            "h": self.height,
            "layers": [l.copy() for l in self.layers],
            "active": self.active,
            "selection": self.selection,
        }

    def restore(self, state: dict):
        self.width = state["w"]
        self.height = state["h"]
        self.layers = [l.copy() for l in state["layers"]]
        self.active = state["active"]
        self.selection = state["selection"]
        self.touch()

    @contextmanager
    def edit(self, text: str):
        """Usage: with doc.edit("Brush"): ...mutate...  (pushes one undo step)."""
        before = self.capture()
        yield
        self.commit(text, before)

    def commit(self, text: str, before: dict):
        after = self.capture()
        self.undo_stack.push(EditCommand(self, text, before, after))
        self.touch()


class EditCommand(QUndoCommand):
    def __init__(self, doc: Document, text: str, before: dict, after: dict):
        super().__init__(text)
        self.doc = doc
        self.before = before
        self.after = after
        self._first = True

    def redo(self):
        # The change was already applied when the command was created.
        if self._first:
            self._first = False
            return
        self.doc.restore(self.after)

    def undo(self):
        self.doc.restore(self.before)
