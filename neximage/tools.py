"""Editing tools. Each tool receives mouse events in *document* coordinates."""
import math

import numpy as np
from PySide6.QtCore import Qt, QPointF, QRectF, QRect, QPoint
from PySide6.QtGui import (QPainter, QPen, QColor, QPainterPath, QRadialGradient, QBrush,
                           QFont, QFontMetrics, QTransform)
from PySide6.QtWidgets import QInputDialog

from . import imgutil as iu
from . import ops
from .document import Selection, Layer, combine_masks


class Ev:
    """Mouse event in document coordinates."""

    def __init__(self, pos: QPointF, button, modifiers):
        self.pos = pos
        self.button = button
        self.mods = modifiers

    @property
    def ix(self) -> int:
        return int(math.floor(self.pos.x()))

    @property
    def iy(self) -> int:
        return int(math.floor(self.pos.y()))

    @property
    def shift(self) -> bool:
        return bool(self.mods & Qt.KeyboardModifier.ShiftModifier)

    @property
    def alt(self) -> bool:
        return bool(self.mods & Qt.KeyboardModifier.AltModifier)

    def op(self) -> str:
        """Selection combine mode from modifiers: Shift=add, Alt=subtract."""
        if self.shift:
            return "add"
        if self.alt:
            return "subtract"
        return "replace"


def _dashed_pen(width: float, color: QColor) -> QPen:
    pen = QPen(color, width)
    pen.setCosmetic(True)
    pen.setStyle(Qt.PenStyle.DashLine)
    return pen


class Tool:
    name = ""
    label = ""
    shortcut = ""
    icon = ""
    cursor = Qt.CursorShape.ArrowCursor
    hint = ""

    def __init__(self, view):
        self.view = view

    @property
    def doc(self):
        return self.view.doc

    @property
    def settings(self):
        return self.view.settings

    def press(self, e: Ev): ...
    def move(self, e: Ev): ...
    def release(self, e: Ev): ...
    def hover(self, e: Ev): ...   # mouse moved with no button pressed

    def key(self, key: int, mods) -> bool:
        return False

    def paint(self, p: QPainter, zoom: float): ...

    def cancel(self): ...

    def deactivate(self):
        self.cancel()


# ------------------------------------------------------------------ move
class MoveTool(Tool):
    name = "move"
    label = "Move"
    shortcut = "V"
    icon = "move"
    cursor = Qt.CursorShape.SizeAllCursor
    hint = "Drag to move the layer. Drag inside a selection to move its pixels. Arrow keys nudge."

    def __init__(self, view):
        super().__init__(view)
        self._start = None
        self._mode = None
        self._before = None
        self._float = None
        self._layer = None
        self._delta = (0, 0)
        self._start_pos = (0, 0)

    def press(self, e: Ev):
        layer = self.doc.active_layer
        if layer is None or e.button != Qt.MouseButton.LeftButton:
            return
        if layer.locked:
            self.view.message("Layer is locked")
            return
        self._layer = layer
        self._start = e.pos
        self._delta = (0, 0)
        self._before = self.doc.capture()
        sel = self.doc.selection
        inside = False
        if sel is not None and 0 <= e.iy < sel.mask.shape[0] and 0 <= e.ix < sel.mask.shape[1]:
            inside = sel.mask[e.iy, e.ix] > 127
        if inside:
            # Lift the selected pixels out of the layer; they follow the mouse.
            mask = ops.selection_window(self.doc, layer)
            rgba = iu.to_rgba(layer.image)
            lifted = rgba.copy()
            lifted[..., 3] = (rgba[..., 3].astype(np.uint16) * mask // 255).astype(np.uint8)
            rgba[..., 3] = (rgba[..., 3].astype(np.uint16) * (255 - mask) // 255).astype(np.uint8)
            layer.image = iu.from_rgba(rgba)
            self._float = iu.from_rgba(lifted)
            self._mode = "pixels"
            self.doc.touch()
        else:
            self._mode = "layer"
            self._start_pos = (layer.x, layer.y)

    def move(self, e: Ev):
        if self._mode is None or self._start is None:
            return
        dx = int(round(e.pos.x() - self._start.x()))
        dy = int(round(e.pos.y() - self._start.y()))
        self._delta = (dx, dy)
        if self._mode == "layer":
            self._layer.x = self._start_pos[0] + dx
            self._layer.y = self._start_pos[1] + dy
            self.doc.touch()
        else:
            self.view.update()

    def release(self, e: Ev):
        if self._mode is None:
            return
        dx, dy = self._delta
        layer = self._layer
        if self._mode == "pixels":
            ox, oy = layer.x, layer.y
            if dx or dy:
                # grow the layer if the moved pixels land outside its bounds
                ops.expand_layer(layer, QRect(ox + dx, oy + dy, layer.w, layer.h))
                sel = self.doc.selection
                if sel is not None:
                    shifted = iu.mask_window(sel.mask, -dx, -dy, sel.mask.shape[1], sel.mask.shape[0])
                    self.doc.selection = Selection(shifted, sel.path.translated(dx, dy))
            p = QPainter(layer.image)
            p.drawImage(ox + dx - layer.x, oy + dy - layer.y, self._float)
            p.end()
            self.doc.commit("Move Selection", self._before)
        elif dx or dy:
            self.doc.commit("Move Layer", self._before)
        self._mode = None
        self._float = None
        self._layer = None
        self._start = None

    def key(self, key, mods) -> bool:
        step = 10 if mods & Qt.KeyboardModifier.ShiftModifier else 1
        d = {Qt.Key.Key_Left: (-step, 0), Qt.Key.Key_Right: (step, 0),
             Qt.Key.Key_Up: (0, -step), Qt.Key.Key_Down: (0, step)}.get(Qt.Key(key))
        layer = self.doc.active_layer
        if d is None or layer is None or layer.locked:
            return False
        before = self.doc.capture()
        layer.x += d[0]
        layer.y += d[1]
        self.doc.commit("Nudge Layer", before)
        return True

    def paint(self, p, zoom):
        if self._mode == "pixels" and self._float is not None and self._layer is not None:
            dx, dy = self._delta
            p.drawImage(self._layer.x + dx, self._layer.y + dy, self._float)


# ------------------------------------------------------------- marquee
class MarqueeTool(Tool):
    name = "marquee"
    label = "Rectangular / Elliptical Marquee"
    shortcut = "M"
    icon = "marquee"
    cursor = Qt.CursorShape.CrossCursor
    hint = "Drag to select. Shift adds to the selection, Alt subtracts. Click to deselect."

    def __init__(self, view):
        super().__init__(view)
        self._start = None
        self._cur = None

    def press(self, e: Ev):
        if e.button != Qt.MouseButton.LeftButton:
            return
        self._start = e.pos
        self._cur = e.pos

    def move(self, e: Ev):
        if self._start is not None:
            self._cur = e.pos
            self.view.update()

    def release(self, e: Ev):
        if self._start is None:
            return
        start, cur = self._start, e.pos
        self._start = None
        self.view.update()
        r = QRectF(start, cur).normalized().intersected(
            QRectF(0, 0, self.doc.width, self.doc.height))
        if r.width() < 1 or r.height() < 1:
            if (cur - start).manhattanLength() < 3 and e.op() == "replace":
                ops.deselect(self.doc)
            return
        path = QPainterPath()
        if self.settings.marquee_shape == "ellipse":
            path.addEllipse(r)
        else:
            path.addRect(r)
        sel = Selection.from_path(path, self.doc.width, self.doc.height)
        ops.set_selection(self.doc, sel, e.op())

    def paint(self, p, zoom):
        if self._start is None:
            return
        r = QRectF(self._start, self._cur).normalized()
        p.setPen(_dashed_pen(1, QColor(255, 255, 255)))
        p.setBrush(Qt.BrushStyle.NoBrush)
        if self.settings.marquee_shape == "ellipse":
            p.drawEllipse(r)
        else:
            p.drawRect(r)


# --------------------------------------------------------------- lasso
class LassoTool(Tool):
    name = "lasso"
    label = "Lasso"
    shortcut = "L"
    icon = "lasso"
    cursor = Qt.CursorShape.CrossCursor
    hint = "Drag freehand around an area to select it."

    def __init__(self, view):
        super().__init__(view)
        self._pts = None

    def press(self, e: Ev):
        if e.button == Qt.MouseButton.LeftButton:
            self._pts = [e.pos]

    def move(self, e: Ev):
        if self._pts is not None:
            self._pts.append(e.pos)
            self.view.update()

    def release(self, e: Ev):
        pts, self._pts = self._pts, None
        self.view.update()
        if not pts or len(pts) < 3:
            return
        path = QPainterPath(pts[0])
        for pt in pts[1:]:
            path.lineTo(pt)
        path.closeSubpath()
        sel = Selection.from_path(path, self.doc.width, self.doc.height)
        ops.set_selection(self.doc, sel, e.op())

    def paint(self, p, zoom):
        if self._pts and len(self._pts) > 1:
            path = QPainterPath(self._pts[0])
            for pt in self._pts[1:]:
                path.lineTo(pt)
            p.setPen(_dashed_pen(1, QColor(255, 255, 255)))
            p.setBrush(Qt.BrushStyle.NoBrush)
            p.drawPath(path)


# -------------------------------------------------------- magic wand
class WandTool(Tool):
    name = "wand"
    label = "Magic Wand"
    shortcut = "W"
    icon = "wand"
    cursor = Qt.CursorShape.CrossCursor
    hint = "Click to select pixels of similar color (tolerance and contiguous in options)."

    def press(self, e: Ev):
        layer = self.doc.active_layer
        if layer is None or e.button != Qt.MouseButton.LeftButton:
            return
        lx, ly = e.ix - layer.x, e.iy - layer.y
        if not (0 <= lx < layer.w and 0 <= ly < layer.h):
            ops.deselect(self.doc) if e.op() == "replace" else None
            return
        rgba = iu.to_rgba(layer.image)
        local = iu.flood_mask(rgba, (lx, ly), self.settings.tolerance, self.settings.contiguous)
        # embed the layer-local mask into canvas space
        full = np.zeros((self.doc.height, self.doc.width), np.uint8)
        x0, y0 = max(0, layer.x), max(0, layer.y)
        x1, y1 = min(self.doc.width, layer.x + layer.w), min(self.doc.height, layer.y + layer.h)
        if x1 > x0 and y1 > y0:
            full[y0:y1, x0:x1] = (local[y0 - layer.y:y1 - layer.y,
                                        x0 - layer.x:x1 - layer.x] * 255).astype(np.uint8)
        sel = Selection.from_mask(full)
        if sel is None:
            ops.deselect(self.doc)
            return
        ops.set_selection(self.doc, sel, e.op())


# ------------------------------------------------------------- crop
class CropTool(Tool):
    name = "crop"
    label = "Crop"
    shortcut = "C"
    icon = "crop"
    cursor = Qt.CursorShape.CrossCursor
    hint = "Drag to choose the crop area, then press Enter to apply (Esc cancels)."

    def __init__(self, view):
        super().__init__(view)
        self.rect: QRectF | None = None
        self._start = None

    def press(self, e: Ev):
        if e.button != Qt.MouseButton.LeftButton:
            return
        self._start = e.pos
        self.rect = QRectF(e.pos, e.pos)

    def move(self, e: Ev):
        if self._start is not None:
            self.rect = QRectF(self._start, e.pos).normalized()
            self.view.update()

    def release(self, e: Ev):
        self._start = None
        if self.rect is not None:
            self.rect = self.rect.intersected(QRectF(0, 0, self.doc.width, self.doc.height))
            if self.rect.width() < 1 or self.rect.height() < 1:
                self.rect = None
        self.view.update()

    def key(self, key, mods) -> bool:
        if key in (Qt.Key.Key_Return, Qt.Key.Key_Enter) and self.rect is not None:
            r = QRect(round(self.rect.x()), round(self.rect.y()),
                      round(self.rect.width()), round(self.rect.height()))
            self.rect = None
            ops.crop_canvas(self.doc, r)
            self.view.fit()
            return True
        if key == Qt.Key.Key_Escape and self.rect is not None:
            self.cancel()
            return True
        return False

    def cancel(self):
        self.rect = None
        self._start = None
        self.view.update()

    def paint(self, p, zoom):
        if self.rect is None:
            return
        full = QPainterPath()
        full.addRect(QRectF(0, 0, self.doc.width, self.doc.height))
        hole = QPainterPath()
        hole.addRect(self.rect)
        p.fillPath(full.subtracted(hole), QColor(0, 0, 0, 150))
        pen = QPen(QColor(255, 255, 255), 1)
        pen.setCosmetic(True)
        p.setPen(pen)
        p.setBrush(Qt.BrushStyle.NoBrush)
        p.drawRect(self.rect)
        p.setPen(_dashed_pen(1, QColor(255, 255, 255, 120)))
        for i in (1, 2):
            x = self.rect.x() + self.rect.width() * i / 3
            y = self.rect.y() + self.rect.height() * i / 3
            p.drawLine(QPointF(x, self.rect.top()), QPointF(x, self.rect.bottom()))
            p.drawLine(QPointF(self.rect.left(), y), QPointF(self.rect.right(), y))


# ---------------------------------------------------- brush / eraser
class BrushTool(Tool):
    name = "brush"
    label = "Brush"
    shortcut = "B"
    icon = "brush"
    cursor = Qt.CursorShape.CrossCursor
    hint = "Paint with the foreground color. [ and ] change the size."
    erase = False

    def __init__(self, view):
        super().__init__(view)
        self._painter = None
        self._layer = None
        self._before = None
        self._last = None

    def _local(self, pt: QPointF) -> QPointF:
        return QPointF(pt.x() - self._layer.x, pt.y() - self._layer.y)

    def press(self, e: Ev):
        layer = self.doc.active_layer
        if layer is None or e.button != Qt.MouseButton.LeftButton:
            return
        if layer.locked:
            self.view.message("Layer is locked")
            return
        self._layer = layer
        self._before = self.doc.capture()
        p = QPainter(layer.image)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        if self.doc.selection is not None:
            p.setClipPath(self.doc.selection.path.translated(-layer.x, -layer.y))
        if self.erase:
            p.setCompositionMode(QPainter.CompositionMode.CompositionMode_DestinationOut)
        self._painter = p
        self._last = e.pos
        self._stamp(e.pos)
        self.doc.touch()

    def move(self, e: Ev):
        if self._painter is None:
            return
        spacing = max(1.0, self.settings.brush_size * 0.12)
        a, b = self._last, e.pos
        dist = math.hypot(b.x() - a.x(), b.y() - a.y())
        steps = max(1, int(dist / spacing))
        for i in range(1, steps + 1):
            t = i / steps
            self._stamp(QPointF(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t))
        self._last = e.pos
        self.doc.touch()

    def release(self, e: Ev):
        if self._painter is None:
            return
        self._painter.end()
        self._painter = None
        self.doc.commit("Eraser" if self.erase else "Brush", self._before)

    def _stamp(self, pt: QPointF):
        s = self.settings
        r = max(0.5, s.brush_size / 2.0)
        color = QColor(0, 0, 0) if self.erase else s.fg
        alpha = int(255 * s.brush_opacity / 100)
        center = self._local(pt)
        grad = QRadialGradient(center, r)
        hard = min(0.999, max(0.0, s.brush_hardness / 100.0))
        inner = QColor(color)
        inner.setAlpha(alpha)
        outer = QColor(color)
        outer.setAlpha(0)
        grad.setColorAt(0.0, inner)
        grad.setColorAt(hard, inner)
        grad.setColorAt(1.0, outer)
        self._painter.setPen(Qt.PenStyle.NoPen)
        self._painter.setBrush(QBrush(grad))
        self._painter.drawEllipse(center, r, r)

    def hover(self, e: Ev):
        self._cursor_pos = e.pos
        self.view.update()

    def paint(self, p, zoom):
        if self._painter is not None or getattr(self, "_cursor_pos", None) is None:
            return
        r = self.settings.brush_size / 2
        pen = QPen(QColor(0, 0, 0, 200), 1)
        pen.setCosmetic(True)
        p.setPen(pen)
        p.setBrush(Qt.BrushStyle.NoBrush)
        p.drawEllipse(self._cursor_pos, r, r)

    def cancel(self):
        if self._painter is not None:
            self._painter.end()
            self._painter = None


class EraserTool(BrushTool):
    name = "eraser"
    label = "Eraser"
    shortcut = "E"
    icon = "eraser"
    hint = "Erase pixels of the active layer."
    erase = True


# ------------------------------------------------------------ bucket
class BucketTool(Tool):
    name = "bucket"
    label = "Paint Bucket"
    shortcut = "G"
    icon = "bucket"
    cursor = Qt.CursorShape.PointingHandCursor
    hint = "Click to fill similar pixels with the foreground color."

    def press(self, e: Ev):
        layer = self.doc.active_layer
        if layer is None or e.button != Qt.MouseButton.LeftButton:
            return
        if layer.locked:
            self.view.message("Layer is locked")
            return
        lx, ly = e.ix - layer.x, e.iy - layer.y
        if not (0 <= lx < layer.w and 0 <= ly < layer.h):
            return
        rgba = iu.to_rgba(layer.image)
        m = iu.flood_mask(rgba, (lx, ly), self.settings.tolerance, self.settings.contiguous)
        sel = ops.selection_window(self.doc, layer)
        if sel is not None:
            m &= sel > 127
        if not m.any():
            return
        before = self.doc.capture()
        c = self.settings.fg
        rgba[m] = (c.red(), c.green(), c.blue(), 255)
        layer.image = iu.from_rgba(rgba)
        self.doc.commit("Paint Bucket", before)


class GradientTool(Tool):
    name = "gradient"
    label = "Gradient"
    shortcut = "Shift+G"
    icon = "gradient"
    cursor = Qt.CursorShape.CrossCursor
    hint = "Drag to draw a foreground-to-background linear gradient."

    def __init__(self, view):
        super().__init__(view)
        self._a = None
        self._b = None

    def press(self, e: Ev):
        if e.button == Qt.MouseButton.LeftButton and self.doc.active_layer is not None:
            self._a = e.pos
            self._b = e.pos

    def move(self, e: Ev):
        if self._a is not None:
            self._b = e.pos
            self.view.update()

    def release(self, e: Ev):
        if self._a is None:
            return
        a, b = self._a, e.pos
        self._a = None
        self.view.update()
        layer = self.doc.active_layer
        if layer is None or layer.locked or math.hypot(b.x() - a.x(), b.y() - a.y()) < 2:
            return
        h, w = layer.h, layer.w
        ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
        X = xs + layer.x + 0.5
        Y = ys + layer.y + 0.5
        dx, dy = b.x() - a.x(), b.y() - a.y()
        t = np.clip(((X - a.x()) * dx + (Y - a.y()) * dy) / (dx * dx + dy * dy), 0, 1)
        fg, bg = self.settings.fg, self.settings.bg
        grad = np.empty((h, w, 4), np.float32)
        for i, (c0, c1) in enumerate([(fg.red(), bg.red()), (fg.green(), bg.green()),
                                      (fg.blue(), bg.blue())]):
            grad[..., i] = c0 + (c1 - c0) * t
        grad[..., 3] = 255
        grad = np.clip(grad + 0.5, 0, 255).astype(np.uint8)
        old = iu.to_rgba(layer.image)
        mask = ops.selection_window(self.doc, layer)
        before = self.doc.capture()
        layer.image = iu.from_rgba(iu.blend_with_mask(old, grad, mask))
        self.doc.commit("Gradient", before)

    def paint(self, p, zoom):
        if self._a is not None and self._b is not None:
            p.setPen(QPen(QColor(255, 255, 255), 1))
            p.drawLine(self._a, self._b)


# ------------------------------------------------------- eyedropper
class EyedropperTool(Tool):
    name = "eyedropper"
    label = "Eyedropper"
    shortcut = "I"
    icon = "eyedropper"
    cursor = Qt.CursorShape.CrossCursor
    hint = "Click to pick a foreground color. Alt-click picks the background color."

    def press(self, e: Ev):
        self._sample(e, e.button == Qt.MouseButton.LeftButton)

    def move(self, e: Ev):
        pass

    def _sample(self, e: Ev, _left):
        img = self.doc.composite()
        x, y = e.ix, e.iy
        if not (0 <= x < img.width() and 0 <= y < img.height()):
            return
        c = img.pixelColor(x, y)
        if c.alpha() == 0:
            return
        c = QColor(c.red(), c.green(), c.blue())
        if e.alt:
            self.settings.bg = c
        else:
            self.settings.fg = c


# ------------------------------------------------------------- text
class TextTool(Tool):
    name = "text"
    label = "Horizontal Type"
    shortcut = "T"
    icon = "text"
    cursor = Qt.CursorShape.IBeamCursor
    hint = "Click on the canvas to add text (creates a text layer)."

    def press(self, e: Ev):
        if e.button != Qt.MouseButton.LeftButton or self.doc.width == 0:
            return
        text, ok = QInputDialog.getMultiLineText(self.view, "Type Tool", "Text:", "")
        if not ok or not text.strip():
            return
        s = self.settings
        font = QFont(s.font_family, s.font_size)
        font.setBold(s.font_bold)
        img = iu.new_image(self.doc.width, self.doc.height)
        p = QPainter(img)
        p.setRenderHint(QPainter.RenderHint.TextAntialiasing)
        p.setFont(font)
        p.setPen(s.fg)
        fm = QFontMetrics(font)
        y = e.pos.y()
        for line in text.split("\n"):
            p.drawText(QPointF(e.pos.x(), y + fm.ascent()), line)
            y += fm.lineSpacing()
        p.end()
        ops.add_layer(self.doc, img, name=text.strip().split("\n")[0][:28])


class ShapeTool(Tool):
    name = "shape"
    label = "Rectangle / Ellipse"
    shortcut = "U"
    icon = "shape"
    cursor = Qt.CursorShape.CrossCursor
    hint = "Drag to draw a filled shape on a new layer."

    def __init__(self, view):
        super().__init__(view)
        self._a = None
        self._b = None

    def press(self, e: Ev):
        if e.button == Qt.MouseButton.LeftButton:
            self._a = e.pos
            self._b = e.pos

    def move(self, e: Ev):
        if self._a is not None:
            self._b = e.pos
            self.view.update()

    def release(self, e: Ev):
        if self._a is None:
            return
        r = QRectF(self._a, e.pos).normalized()
        self._a = None
        self.view.update()
        if r.width() < 2 or r.height() < 2:
            return
        img = iu.new_image(self.doc.width, self.doc.height)
        p = QPainter(img)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(self.settings.fg)
        if self.settings.shape_kind == "ellipse":
            p.drawEllipse(r)
        else:
            p.drawRect(r)
        p.end()
        kind = "Ellipse" if self.settings.shape_kind == "ellipse" else "Rectangle"
        ops.add_layer(self.doc, img, name=kind)

    def paint(self, p, zoom):
        if self._a is None:
            return
        r = QRectF(self._a, self._b).normalized()
        p.setPen(_dashed_pen(1, QColor(255, 255, 255)))
        p.setBrush(Qt.BrushStyle.NoBrush)
        if self.settings.shape_kind == "ellipse":
            p.drawEllipse(r)
        else:
            p.drawRect(r)


# -------------------------------------------------------- hand / zoom
class HandTool(Tool):
    name = "hand"
    label = "Hand"
    shortcut = "H"
    icon = "hand"
    cursor = Qt.CursorShape.OpenHandCursor
    hint = "Drag to pan the view (hold Space with any tool)."
    pan_handled = True  # the view performs the pan itself


class ZoomTool(Tool):
    name = "zoom"
    label = "Zoom"
    shortcut = "Z"
    icon = "zoom"
    cursor = Qt.CursorShape.CrossCursor
    hint = "Click to zoom in (Alt-click to zoom out) or drag a rectangle to zoom to it."

    def __init__(self, view):
        super().__init__(view)
        self._a = None
        self._b = None

    def press(self, e: Ev):
        if e.button == Qt.MouseButton.LeftButton:
            self._a = e.pos
            self._b = e.pos

    def move(self, e: Ev):
        if self._a is not None:
            self._b = e.pos
            self.view.update()

    def release(self, e: Ev):
        if self._a is None:
            return
        a = self._a
        self._a = None
        r = QRectF(a, e.pos).normalized()
        if r.width() > 6 and r.height() > 6:
            self.view.zoom_to_rect(r)
        else:
            factor = 1 / 1.25 if e.alt else 1.25
            self.view.set_zoom(self.view.zoom * factor, e.pos)
        self.view.update()

    def paint(self, p, zoom):
        if self._a is not None and self._b is not None:
            p.setPen(_dashed_pen(1, QColor(255, 255, 255)))
            p.setBrush(Qt.BrushStyle.NoBrush)
            p.drawRect(QRectF(self._a, self._b).normalized())


# ------------------------------------------------------ free transform
class TransformTool(Tool):
    """Ctrl+T: scale/move the active layer inside a bounding box."""
    name = "transform"
    label = "Free Transform"
    shortcut = "Ctrl+T"
    icon = "transform"
    cursor = Qt.CursorShape.SizeAllCursor
    hint = "Drag handles to scale, drag inside to move. Enter applies, Esc cancels."
    HANDLES = ["tl", "tc", "tr", "ml", "mr", "bl", "bc", "br"]

    def __init__(self, view):
        super().__init__(view)
        self.active = False
        self.box = QRectF()
        self._before = None
        self._layer = None
        self._drag = None
        self._start_pt = None
        self._start_box = None

    def begin(self) -> bool:
        layer = self.doc.active_layer
        if layer is None or layer.locked:
            self.view.message("Select an unlocked layer first")
            return False
        self._layer = layer
        self._before = self.doc.capture()
        self.box = QRectF(layer.x, layer.y, layer.w, layer.h)
        self.active = True
        self.doc.preview_hidden = layer
        self.doc.touch()
        return True

    def _handle_points(self):
        b = self.box
        return {
            "tl": b.topLeft(), "tr": b.topRight(), "bl": b.bottomLeft(), "br": b.bottomRight(),
            "tc": QPointF(b.center().x(), b.top()), "bc": QPointF(b.center().x(), b.bottom()),
            "ml": QPointF(b.left(), b.center().y()), "mr": QPointF(b.right(), b.center().y()),
        }

    def _hit(self, pt: QPointF):
        tol = 8 / self.view.zoom
        for name, hp in self._handle_points().items():
            if math.hypot(pt.x() - hp.x(), pt.y() - hp.y()) <= tol:
                return name
        return "move" if self.box.contains(pt) else None

    def press(self, e: Ev):
        if not self.active or e.button != Qt.MouseButton.LeftButton:
            return
        self._drag = self._hit(e.pos)
        self._start_pt = e.pos
        self._start_box = QRectF(self.box)

    def move(self, e: Ev):
        if not self.active or self._drag is None:
            return
        b = QRectF(self._start_box)
        dx = e.pos.x() - self._start_pt.x()
        dy = e.pos.y() - self._start_pt.y()
        d = self._drag
        if d == "move":
            b.translate(dx, dy)
        else:
            left, top, right, bottom = b.left(), b.top(), b.right(), b.bottom()
            if d in ("tl", "ml", "bl"):
                left += dx
            if d in ("tr", "mr", "br"):
                right += dx
            if d in ("tl", "tc", "tr"):
                top += dy
            if d in ("bl", "bc", "br"):
                bottom += dy
            left, top = min(left, right - 1), min(top, bottom - 1)
            right, bottom = max(right, left + 1), max(bottom, top + 1)
            b = QRectF(QPointF(left, top), QPointF(right, bottom))
        self.box = b
        self.view.update()

    def release(self, e: Ev):
        self._drag = None

    def key(self, key, mods) -> bool:
        if not self.active:
            return False
        if key in (Qt.Key.Key_Return, Qt.Key.Key_Enter):
            layer = self._layer
            before = self._before
            self.active = False
            self.doc.preview_hidden = None
            ops.apply_free_transform(self.doc, layer, self.box, before)
            return True
        if key == Qt.Key.Key_Escape:
            self.cancel()
            return True
        return False

    def cancel(self):
        if self.active:
            self.active = False
            self.doc.preview_hidden = None
            self.doc.touch()
        self._drag = None

    def paint(self, p, zoom):
        if not self.active or self._layer is None:
            return
        p.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform)
        p.drawImage(self.box, self._layer.image)
        pen = QPen(QColor(0, 160, 255), 1)
        pen.setCosmetic(True)
        p.setPen(pen)
        p.setBrush(Qt.BrushStyle.NoBrush)
        p.drawRect(self.box)
        size = 8 / zoom
        p.setBrush(QColor(255, 255, 255))
        for hp in self._handle_points().values():
            p.drawRect(QRectF(hp.x() - size / 2, hp.y() - size / 2, size, size))


TOOL_CLASSES = [MoveTool, MarqueeTool, LassoTool, WandTool, CropTool, EyedropperTool,
                BrushTool, EraserTool, BucketTool, GradientTool, TextTool, ShapeTool,
                HandTool, ZoomTool]
TOOLS_BY_NAME = {cls.name: cls for cls in TOOL_CLASSES + [TransformTool]}
