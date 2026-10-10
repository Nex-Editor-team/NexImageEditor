"""Document operations. Each public function performs one undoable edit."""
import numpy as np
from PySide6.QtCore import QRect, QPoint, QRectF, Qt
from PySide6.QtGui import QImage, QPainter, QTransform, QColor

from . import imgutil as iu
from .document import Document, Layer, Selection, combine_masks, render_stack, BLEND_QT


# ---------------------------------------------------------------- layers
def add_layer(doc: Document, image: QImage | None = None, name: str | None = None,
              above_active: bool = True) -> None:
    """Add a layer. Without an image it is a transparent canvas-sized layer."""
    with doc.edit("New Layer"):
        if image is None:
            image = iu.new_image(doc.width, doc.height)
        layer = Layer(image, name or doc.new_layer_name(), 0, 0)
        index = doc.active + 1 if above_active and doc.active >= 0 else len(doc.layers)
        doc.layers.insert(index, layer)
        doc.active = index


def delete_layer(doc: Document, index: int | None = None) -> None:
    index = doc.active if index is None else index
    if not (0 <= index < len(doc.layers)):
        return
    with doc.edit("Delete Layer"):
        del doc.layers[index]
        doc.active = min(index, len(doc.layers) - 1)


def duplicate_layer(doc: Document) -> None:
    layer = doc.active_layer
    if layer is None:
        return
    with doc.edit("Duplicate Layer"):
        dup = layer.copy()
        dup.name = f"{layer.name} copy"
        dup.background = False
        doc.layers.insert(doc.active + 1, dup)
        doc.active += 1


def move_layer(doc: Document, delta: int) -> None:
    i = doc.active
    j = i + delta
    if not (0 <= i < len(doc.layers)) or not (0 <= j < len(doc.layers)):
        return
    with doc.edit("Move Layer"):
        doc.layers[i], doc.layers[j] = doc.layers[j], doc.layers[i]
        doc.active = j


def merge_down(doc: Document) -> None:
    i = doc.active
    if i <= 0 or i >= len(doc.layers):
        return
    lower, upper = doc.layers[i - 1], doc.layers[i]
    left = min(lower.x, upper.x)
    top = min(lower.y, upper.y)
    right = max(lower.x + lower.w, upper.x + upper.w)
    bottom = max(lower.y + lower.h, upper.y + upper.h)
    w, h = right - left, bottom - top
    with doc.edit("Merge Down"):
        img = iu.new_image(w, h)
        p = QPainter(img)
        for layer in (lower, upper):
            p.setOpacity(layer.opacity if layer is upper else 1.0)
            p.setCompositionMode(BLEND_QT[layer.blend] if layer is upper
                                 else BLEND_QT["Normal"])
            p.drawImage(layer.x - left, layer.y - top, layer.image)
        p.end()
        merged = Layer(img, lower.name, left, top)
        merged.visible = lower.visible
        merged.background = lower.background
        doc.layers[i - 1:i + 1] = [merged]
        doc.active = i - 1


def flatten(doc: Document) -> None:
    if len(doc.layers) <= 1:
        return
    with doc.edit("Flatten Image"):
        img = render_stack([l for l in doc.layers if l.visible], doc.width, doc.height)
        bg = Layer(img, "Background", 0, 0, background=True)
        doc.layers = [bg]
        doc.active = 0


def set_layer_props(doc: Document, layer: Layer, undo_text: str, **props) -> None:
    """Change layer attributes (name, opacity, blend, visible, locked, x, y)."""
    if all(getattr(layer, k) == v for k, v in props.items()):
        return
    with doc.edit(undo_text):
        for k, v in props.items():
            setattr(layer, k, v)


def expand_layer(layer: Layer, rect: QRect) -> None:
    """Grow a layer's pixel bounds so that it covers `rect` (canvas coords)."""
    union = layer.rect().united(rect)
    if union == layer.rect():
        return
    img = iu.new_image(union.width(), union.height())
    p = QPainter(img)
    p.drawImage(layer.x - union.x(), layer.y - union.y(), layer.image)
    p.end()
    layer.image = img
    layer.x, layer.y = union.x(), union.y()


# ------------------------------------------------------------- selection
def set_selection(doc: Document, sel: Selection | None, op: str = "replace") -> None:
    with doc.edit("Select"):
        if sel is None:
            doc.selection = None
        else:
            doc.selection = combine_masks(doc.selection, sel, op, doc.width, doc.height)


def select_all(doc: Document) -> None:
    rect = doc.canvas_rect()
    from PySide6.QtGui import QPainterPath
    path = QPainterPath()
    path.addRect(QRectF(rect))
    set_selection(doc, Selection.from_path(path, doc.width, doc.height))


def deselect(doc: Document) -> None:
    if doc.selection is not None:
        set_selection(doc, None)


def invert_selection(doc: Document) -> None:
    if doc.selection is None:
        select_all(doc)
        return
    inv = 255 - doc.selection.mask
    with doc.edit("Inverse Selection"):
        doc.selection = Selection.from_mask(inv)


def selection_window(doc: Document, layer: Layer) -> np.ndarray | None:
    """Selection mask aligned with the layer's pixels (None = no selection)."""
    if doc.selection is None:
        return None
    return iu.mask_window(doc.selection.mask, layer.x, layer.y, layer.w, layer.h)


# ------------------------------------------------------------ pixel ops
def apply_pixels(doc: Document, fn, undo_text: str, layer: Layer | None = None) -> bool:
    """Apply `fn(rgba) -> rgba` to a layer, honoring the active selection."""
    layer = layer or doc.active_layer
    if layer is None or layer.locked:
        return False
    before = doc.capture()
    old = iu.to_rgba(layer.image)
    new = fn(old.copy())
    mask = selection_window(doc, layer)
    layer.image = iu.from_rgba(iu.blend_with_mask(old, new, mask))
    doc.commit(undo_text, before)
    return True


def fill_selection(doc: Document, color: QColor, undo_text: str = "Fill") -> None:
    def fn(rgba):
        rgba[..., 0], rgba[..., 1], rgba[..., 2], rgba[..., 3] = (
            color.red(), color.green(), color.blue(), 255)
        return rgba
    apply_pixels(doc, fn, undo_text)


def clear_selection(doc: Document) -> None:
    def fn(rgba):
        rgba[..., 3] = 0
        return rgba
    apply_pixels(doc, fn, "Clear")


def selection_image(doc: Document, merged: bool = False) -> QImage | None:
    """Pixels inside the selection (or the whole layer / image) as RGBA."""
    layer = doc.active_layer
    if merged:
        source, origin = doc.composite(), QRect(0, 0, doc.width, doc.height)
        origin_xy = (0, 0)
    else:
        if layer is None:
            return None
        source, origin = layer.image, layer.rect()
        origin_xy = (layer.x, layer.y)
    canvas_bounds = origin
    if doc.selection is not None:
        canvas_bounds = doc.selection.bounds().intersected(origin)
    if canvas_bounds.isEmpty():
        return None
    local = canvas_bounds.translated(-origin_xy[0], -origin_xy[1])
    img = source.copy(local)
    rgba = iu.to_rgba(img)
    if doc.selection is not None:
        mask = doc.selection.mask[canvas_bounds.y():canvas_bounds.y() + canvas_bounds.height(),
                                  canvas_bounds.x():canvas_bounds.x() + canvas_bounds.width()]
        rgba[..., 3] = (rgba[..., 3].astype(np.uint16) * mask // 255).astype(np.uint8)
    return iu.from_rgba(rgba)


def paste_as_layer(doc: Document, image: QImage, center: bool = True,
                   name: str = "Pasted Layer") -> None:
    """Paste pixels as a new canvas-sized layer, centered when requested."""
    x = (doc.width - image.width()) // 2 if center else 0
    y = (doc.height - image.height()) // 2 if center else 0
    canvas = iu.new_image(doc.width, doc.height)
    p = QPainter(canvas)
    p.drawImage(x, y, image)
    p.end()
    add_layer(doc, canvas, name=name)


def layer_via_copy(doc: Document) -> None:
    """Ctrl+J: copy the selection (or whole layer) into a new layer above."""
    img = selection_image(doc)
    layer = doc.active_layer
    if img is None or layer is None:
        return
    canvas = iu.new_image(doc.width, doc.height)
    bounds = (doc.selection.bounds() if doc.selection else layer.rect())
    p = QPainter(canvas)
    p.drawImage(bounds.x(), bounds.y(), img)
    p.end()
    add_layer(doc, canvas, name=f"{layer.name} copy")


# ---------------------------------------------------------- transforms
def crop_canvas(doc: Document, rect: QRect) -> None:
    rect = rect.intersected(doc.canvas_rect())
    if rect.isEmpty():
        return
    with doc.edit("Crop"):
        for layer in doc.layers:
            inter = layer.rect().intersected(rect)
            if inter.isEmpty():
                layer.image = iu.new_image(1, 1)
                layer.x = layer.y = 0
                continue
            sub = layer.image.copy(inter.translated(-layer.x, -layer.y))
            layer.image = sub
            layer.x = inter.x() - rect.x()
            layer.y = inter.y() - rect.y()
        doc.width, doc.height = rect.width(), rect.height()
        doc.selection = None


ANCHORS = ["Top Left", "Top Center", "Top Right", "Middle Left", "Center",
           "Middle Right", "Bottom Left", "Bottom Center", "Bottom Right"]


def resize_canvas(doc: Document, w: int, h: int, anchor: str = "Center") -> None:
    col = ANCHORS.index(anchor) % 3
    row = ANCHORS.index(anchor) // 3
    dx = {0: 0, 1: (w - doc.width) // 2, 2: w - doc.width}[col]
    dy = {0: 0, 1: (h - doc.height) // 2, 2: h - doc.height}[row]
    with doc.edit("Canvas Size"):
        for layer in doc.layers:
            layer.x += dx
            layer.y += dy
        doc.width, doc.height = int(w), int(h)
        doc.selection = None


def resample_image(doc: Document, w: int, h: int) -> None:
    sx, sy = w / doc.width, h / doc.height
    with doc.edit("Image Size"):
        for layer in doc.layers:
            nw = max(1, round(layer.w * sx))
            nh = max(1, round(layer.h * sy))
            layer.image = iu.to_premult(layer.image.scaled(
                nw, nh, Qt.AspectRatioMode.IgnoreAspectRatio,
                Qt.TransformationMode.SmoothTransformation))
            layer.x = round(layer.x * sx)
            layer.y = round(layer.y * sy)
        doc.width, doc.height = int(w), int(h)
        doc.selection = None


def rotate_canvas(doc: Document, degrees: int) -> None:
    """degrees: 90 (clockwise), -90 (counter-clockwise) or 180."""
    W, H = doc.width, doc.height
    with doc.edit(f"Rotate {degrees % 360}°"):
        t = QTransform().rotate(degrees)
        for layer in doc.layers:
            lx, ly, lw, lh = layer.x, layer.y, layer.w, layer.h
            if degrees == 90:
                layer.x, layer.y = H - (ly + lh), lx
            elif degrees == -90:
                layer.x, layer.y = ly, W - (lx + lw)
            else:
                layer.x, layer.y = W - (lx + lw), H - (ly + lh)
            layer.image = layer.image.transformed(t, Qt.TransformationMode.SmoothTransformation)
        if degrees == 90 or degrees == -90:
            doc.width, doc.height = H, W
        doc.selection = None


def flip_canvas(doc: Document, horizontal: bool) -> None:
    W, H = doc.width, doc.height
    with doc.edit("Flip Horizontal" if horizontal else "Flip Vertical"):
        for layer in doc.layers:
            if horizontal:
                layer.x = W - (layer.x + layer.w)
            else:
                layer.y = H - (layer.y + layer.h)
            layer.image = layer.image.mirrored(horizontal, not horizontal)
        doc.selection = None


def flip_layer(doc: Document, horizontal: bool) -> None:
    layer = doc.active_layer
    if layer is None or layer.locked:
        return
    with doc.edit("Flip Layer"):
        if horizontal:
            layer.x = doc.width - (layer.x + layer.w)
        else:
            layer.y = doc.height - (layer.y + layer.h)
        layer.image = layer.image.mirrored(horizontal, not horizontal)


def apply_free_transform(doc: Document, layer: Layer, box: QRectF, before: dict) -> None:
    """Scale the layer so that its bounds become `box` (canvas coords)."""
    nw = max(1, round(box.width()))
    nh = max(1, round(box.height()))
    img = iu.new_image(nw, nh)
    p = QPainter(img)
    p.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform)
    p.drawImage(QRectF(0, 0, nw, nh), layer.image)
    p.end()
    layer.image = img
    layer.x = round(box.x())
    layer.y = round(box.y())
    doc.commit("Free Transform", before)
