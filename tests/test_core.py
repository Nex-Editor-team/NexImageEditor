"""Core behaviour tests (no GUI event loop needed; runs offscreen)."""
import os
import sys
import tempfile
import zipfile

import numpy as np
from PySide6.QtCore import QPointF, QRect, QRectF, Qt
from PySide6.QtGui import QColor, QImage, QPainterPath, QPainter
from PySide6.QtWidgets import QApplication

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from neximage import adjustments as adj  # noqa: E402
from neximage import fileio, ops  # noqa: E402
from neximage import imgutil as iu  # noqa: E402
from neximage.document import Document, Layer, Selection  # noqa: E402

app = QApplication.instance() or QApplication([])


def solid(w, h, color):
    img = iu.new_image(w, h, color)
    return img


def pixel(img, x, y):
    c = img.pixelColor(x, y)
    return (c.red(), c.green(), c.blue(), c.alpha())


def test_rgba_roundtrip():
    arr = np.zeros((4, 5, 4), np.uint8)
    arr[..., 0] = 200
    arr[..., 3] = 255
    arr[1, 2] = (10, 20, 30, 128)
    img = iu.from_rgba(arr)
    back = iu.to_rgba(img)
    assert back.shape == arr.shape
    assert np.array_equal(back[..., 3], arr[..., 3])
    assert np.abs(back[0, 0].astype(int) - arr[0, 0].astype(int)).max() <= 1


def test_undo_redo_restores_pixels():
    doc = Document(10, 10, background=QColor(255, 255, 255))
    layer = doc.active_layer
    before_px = pixel(layer.image, 3, 3)
    ops.fill_selection(doc, QColor(255, 0, 0))            # no selection: fills layer
    assert pixel(doc.active_layer.image, 3, 3) == (255, 0, 0, 255)
    doc.undo_stack.undo()
    assert pixel(doc.active_layer.image, 3, 3) == before_px
    doc.undo_stack.redo()
    assert pixel(doc.active_layer.image, 3, 3) == (255, 0, 0, 255)


def test_snapshot_isolated_from_inplace_painting():
    """Painting in place after capture must not change the captured snapshot."""
    doc = Document(20, 20, background=QColor(255, 255, 255))
    state = doc.capture()
    p = QPainter(doc.active_layer.image)
    p.fillRect(0, 0, 20, 20, QColor(0, 0, 0))
    p.end()
    assert pixel(doc.active_layer.image, 5, 5) == (0, 0, 0, 255)
    assert pixel(state["layers"][0].image, 5, 5) == (255, 255, 255, 255)


def test_selection_limits_fill():
    doc = Document(20, 20, background=QColor(255, 255, 255))
    path = QPainterPath()
    path.addRect(QRectF(5, 5, 10, 10))
    ops.set_selection(doc, Selection.from_path(path, 20, 20))
    ops.fill_selection(doc, QColor(0, 0, 255))
    assert pixel(doc.active_layer.image, 7, 7) == (0, 0, 255, 255)
    assert pixel(doc.active_layer.image, 1, 1) == (255, 255, 255, 255)
    ops.invert_selection(doc)
    assert doc.selection.mask[7, 7] == 0 and doc.selection.mask[1, 1] == 255


def test_magic_wand_flood_selects_region():
    doc = Document(20, 20, background=QColor(255, 255, 255))
    p = QPainter(doc.active_layer.image)
    p.fillRect(0, 0, 4, 20, QColor(0, 0, 0))       # black stripe on the left
    p.end()
    rgba = iu.to_rgba(doc.active_layer.image)
    m = iu.flood_mask(rgba, (1, 1), 10, True)
    assert m[1, 1] and m[10, 0] and not m[10, 10]
    m2 = iu.flood_mask(rgba, (1, 1), 10, False)   # global: same black pixels only
    assert m2[10, 0] and not m2[10, 10]


def test_layer_offsets_and_composite():
    doc = Document(10, 10, background=QColor(0, 0, 0))
    top = Layer(solid(4, 4, QColor(255, 0, 0)), "Top", 3, 3)
    doc.layers.append(top)
    doc.active = 1
    img = doc.composite()
    assert pixel(img, 4, 4) == (255, 0, 0, 255)
    assert pixel(img, 0, 0) == (0, 0, 0, 255)
    top.visible = False
    doc.touch()
    assert pixel(doc.composite(), 4, 4) == (0, 0, 0, 255)


def test_blend_mode_multiply():
    doc = Document(4, 4, background=QColor(128, 128, 128))
    top = Layer(solid(4, 4, QColor(255, 255, 255)), "Top")
    top.blend = "Multiply"
    doc.layers.append(top)
    out = doc.composite()
    assert pixel(out, 0, 0)[0] == 128  # white * grey = grey


def test_merge_down_and_flatten():
    doc = Document(8, 8, background=QColor(255, 255, 255))
    doc.layers.append(Layer(solid(2, 2, QColor(0, 255, 0)), "Green", 1, 1))
    doc.active = 1
    ops.merge_down(doc)
    assert len(doc.layers) == 1
    assert pixel(doc.layers[0].image, 1, 1) == (0, 255, 0, 255)
    ops.add_layer(doc)
    ops.flatten(doc)
    assert len(doc.layers) == 1 and doc.layers[0].background


def test_crop_canvas_shifts_layers():
    doc = Document(10, 10, background=QColor(255, 255, 255))
    ops.crop_canvas(doc, QRect(2, 3, 5, 4))
    assert (doc.width, doc.height) == (5, 4)
    assert doc.layers[0].image.width() == 5 and doc.layers[0].image.height() == 4


def test_resize_canvas_anchor_center():
    doc = Document(10, 10, background=QColor(255, 255, 255))
    ops.resize_canvas(doc, 20, 20, "Center")
    assert (doc.width, doc.height) == (20, 20)
    assert (doc.layers[0].x, doc.layers[0].y) == (5, 5)


def test_rotate_canvas_90_moves_layer():
    doc = Document(10, 6, background=None)   # transparent layer
    doc.layers[0].image = solid(2, 2, QColor(255, 0, 0))
    doc.layers[0].x, doc.layers[0].y = 0, 0
    ops.rotate_canvas(doc, 90)
    assert (doc.width, doc.height) == (6, 10)
    layer = doc.layers[0]
    # top-left block of a 10x6 canvas ends up at top-right of the 6x10 canvas
    assert (layer.x, layer.y) == (6 - 2, 0)


def test_flip_canvas_horizontal():
    doc = Document(10, 4, background=None)
    doc.layers[0].image = solid(2, 2, QColor(0, 255, 0))
    doc.layers[0].x = 0
    ops.flip_canvas(doc, True)
    assert doc.layers[0].x == 10 - 2


def test_resample_image_doubles_layers():
    doc = Document(10, 10, background=QColor(255, 255, 255))
    ops.resample_image(doc, 20, 20)
    assert (doc.width, doc.height) == (20, 20)
    assert doc.layers[0].image.width() == 20


def test_layer_via_copy_uses_selection():
    doc = Document(20, 20, background=None)
    doc.layers[0].image = solid(20, 20, QColor(9, 9, 9))
    path = QPainterPath()
    path.addRect(QRectF(0, 0, 5, 5))
    ops.set_selection(doc, Selection.from_path(path, 20, 20))
    ops.layer_via_copy(doc)
    assert len(doc.layers) == 2
    assert pixel(doc.layers[1].image, 2, 2)[3] == 255
    assert pixel(doc.layers[1].image, 10, 10)[3] == 0


def test_brightness_contrast_and_invert():
    arr = np.full((2, 2, 4), 100, np.uint8)
    arr[..., 3] = 255
    b = adj.brightness_contrast(arr, 50, 0)
    assert int(b[0, 0, 0]) > 100
    inv = adj.invert(arr)
    assert int(inv[0, 0, 0]) == 155
    assert adj.desaturate(arr)[0, 0, 0] == 100


def test_gaussian_blur_keeps_shape_and_alpha():
    arr = np.zeros((16, 16, 4), np.uint8)
    arr[8, 8] = (255, 255, 255, 255)
    out = adj.gaussian_blur(arr, 2.0)
    assert out.shape == arr.shape
    assert out[8, 8, 3] < 255 and out[8, 9, 3] > 0


def test_project_save_load_roundtrip():
    doc = Document(12, 9, background=QColor(10, 20, 30))
    doc.layers.append(Layer(solid(3, 3, QColor(200, 0, 0)), "Red", 4, 2))
    doc.layers[1].opacity = 0.5
    doc.layers[1].blend = "Screen"
    doc.layers[1].locked = True
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "p.nxe")
        fileio.save_project(doc, path)
        with zipfile.ZipFile(path) as z:
            assert "project.json" in z.namelist()
        loaded = fileio.load_project(path)
    assert (loaded.width, loaded.height) == (12, 9)
    assert len(loaded.layers) == 2
    red = loaded.layers[1]
    assert (red.name, red.x, red.y, red.opacity, red.blend, red.locked) == \
        ("Red", 4, 2, 0.5, "Screen", True)
    assert pixel(red.image, 0, 0) == (200, 0, 0, 255)


def test_export_formats():
    doc = Document(6, 6, background=QColor(255, 255, 255))
    with tempfile.TemporaryDirectory() as tmp:
        for ext in (".png", ".jpg"):
            path = os.path.join(tmp, "out" + ext)
            fileio.export_image(doc, path)
            img = QImage(path)
            assert not img.isNull() and img.width() == 6


def test_move_layer_and_nudge_via_ops():
    doc = Document(10, 10, background=QColor(255, 255, 255))
    doc.layers.append(Layer(solid(2, 2, QColor(0, 0, 0)), "L", 0, 0))
    doc.active = 1
    ops.set_layer_props(doc, doc.layers[1], "Move", x=5, y=6)
    assert (doc.layers[1].x, doc.layers[1].y) == (5, 6)
    doc.undo_stack.undo()
    assert (doc.layers[1].x, doc.layers[1].y) == (0, 0)


if __name__ == "__main__":
    fails = 0
    for name, fn in list(globals().items()):
        if name.startswith("test_") and callable(fn):
            try:
                fn()
                print("PASS", name)
            except Exception as exc:  # noqa: BLE001
                fails += 1
                import traceback
                traceback.print_exc()
                print("FAIL", name, exc)
    sys.exit(1 if fails else 0)
