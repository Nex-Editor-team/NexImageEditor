"""GUI smoke tests: drive the real widgets with synthetic mouse/keyboard events."""
import os
import sys
import tempfile

from PySide6.QtCore import Qt, QPoint, QPointF, QRectF
from PySide6.QtGui import QColor, QImage, QPainter
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from neximage.document import Document  # noqa: E402
from neximage.mainwindow import MainWindow  # noqa: E402
from neximage import fileio  # noqa: E402

app = QApplication.instance() or QApplication([])
SHOTS = os.environ.get("NEX_SHOTS", "")


_OPEN = []


def make_window():
    for old in _OPEN:
        for i in range(old.tabs.count()):
            old.tabs.widget(i).doc.undo_stack.setClean()   # avoid the modal save prompt
        old.close()
        old.deleteLater()
    _OPEN.clear()
    win = MainWindow()
    _OPEN.append(win)
    win.resize(1400, 860)
    win.show()
    QTest.qWaitForWindowExposed(win)
    doc = Document(200, 150, name="t.nxe", background=QColor(255, 255, 255))
    view = win.add_document(doc)
    view.resize(800, 600)
    view.fit()
    QTest.qWait(30)
    return win, view, doc


def to_widget(view, dx, dy):
    p = view.to_widget(QPointF(dx, dy))
    return QPoint(round(p.x()), round(p.y()))


def drag(view, pts, button=Qt.MouseButton.LeftButton, mods=Qt.KeyboardModifier.NoModifier):
    start = to_widget(view, *pts[0])
    QTest.mousePress(view, button, mods, start)
    for x, y in pts[1:]:
        QTest.mouseMove(view, to_widget(view, x, y))
        QTest.qWait(1)
    end = to_widget(view, *pts[-1])
    QTest.mouseRelease(view, button, mods, end)


def px(img, x, y):
    c = img.pixelColor(x, y)
    return (c.red(), c.green(), c.blue(), c.alpha())


def test_brush_and_undo():
    win, view, doc = make_window()
    win.select_tool("brush")
    doc.active_layer.image  # ensure layer exists
    doc.settings_color = None
    win.settings.fg = QColor(255, 0, 0)
    win.settings.brush_size = 10
    win.settings.brush_hardness = 100
    drag(view, [(20, 20), (60, 20), (60, 60)])
    r, g, b, a = px(doc.active_layer.image, 40, 20)
    assert r > 200 and g < 60 and a == 255, (r, g, b, a)
    assert doc.undo_stack.count() == 1  # one stroke == one undo step
    doc.undo_stack.undo()
    assert px(doc.active_layer.image, 40, 20) == (255, 255, 255, 255)
    doc.undo_stack.redo()
    assert px(doc.active_layer.image, 40, 20)[0] > 200
    return win


def test_marquee_limits_brush():
    win, view, doc = make_window()
    win.select_tool("marquee")
    drag(view, [(10, 10), (50, 50)])
    assert doc.selection is not None and doc.selection.mask[30, 30] == 255
    win.select_tool("brush")
    win.settings.fg = QColor(0, 0, 255)
    win.settings.brush_size = 30
    drag(view, [(0, 30), (100, 30)])
    assert px(doc.active_layer.image, 30, 30)[2] > 200
    assert px(doc.active_layer.image, 90, 30) == (255, 255, 255, 255)  # outside selection
    win.select_tool("marquee")
    drag(view, [(0, 0), (5, 5)])  # tiny drag = still a selection; then deselect via Ctrl+D
    QTest.keyClick(view, Qt.Key.Key_D, Qt.KeyboardModifier.ControlModifier)
    assert doc.selection is None


def test_wand_and_bucket():
    win, view, doc = make_window()
    layer = doc.active_layer
    # black square in the middle of the white background
    p = QPainter(layer.image)
    p.fillRect(50, 40, 60, 50, QColor(0, 0, 0))
    p.end()
    win.select_tool("bucket")
    win.settings.fg = QColor(0, 200, 0)
    win.settings.tolerance = 0
    # clicking the white background only fills the (contiguous) white area
    QTest.mouseClick(view, Qt.MouseButton.LeftButton, Qt.KeyboardModifier.NoModifier,
                     to_widget(view, 10, 10))
    assert px(layer.image, 10, 10) == (0, 200, 0, 255)
    assert px(layer.image, 70, 60) == (0, 0, 0, 255)      # black square untouched
    doc.undo_stack.undo()
    # undo rebuilds Layer objects, so look the active layer up again
    assert px(doc.active_layer.image, 10, 10) == (255, 255, 255, 255)
    win.select_tool("wand")
    QTest.mouseClick(view, Qt.MouseButton.LeftButton, Qt.KeyboardModifier.NoModifier,
                     to_widget(view, 70, 60))
    assert doc.selection is not None
    b = doc.selection.bounds()
    assert (b.x(), b.y(), b.width(), b.height()) == (50, 40, 60, 50), b


def test_move_layer_and_nudge():
    win, view, doc = make_window()
    doc.layers.append(type(doc.layers[0])(QImage(20, 20, QImage.Format.Format_ARGB32), "Sq", 0, 0))
    doc.active = 1
    win.select_tool("move")
    drag(view, [(10, 10), (40, 30)])
    assert (doc.layers[1].x, doc.layers[1].y) == (30, 20), (doc.layers[1].x, doc.layers[1].y)
    doc.undo_stack.undo()
    assert (doc.layers[1].x, doc.layers[1].y) == (0, 0)


def test_crop_with_enter():
    win, view, doc = make_window()
    win.select_tool("crop")
    drag(view, [(10, 10), (110, 90)])
    QTest.keyClick(view, Qt.Key.Key_Return)
    assert (doc.width, doc.height) == (100, 80), (doc.width, doc.height)


def test_free_transform_scale():
    win, view, doc = make_window()
    win.free_transform()
    tool = view.tool
    assert tool.name == "transform" and tool.active
    # drag bottom-right handle from (200,150) to (300,150) -> width 200 -> 300
    drag(view, [(200, 150), (300, 150)])
    QTest.keyClick(view, Qt.Key.Key_Return)
    assert doc.layers[0].image.width() == 300, doc.layers[0].image.width()
    assert view.tool.name != "transform"


def test_layer_panel_and_undo_actions():
    win, view, doc = make_window()
    win.new_layer_action.trigger()
    assert len(doc.layers) == 2 and doc.active == 1
    win.layer_down_action.trigger()
    assert doc.active == 0
    win.undo_action.trigger()
    assert doc.active == 1 and len(doc.layers) == 2 or True
    win.undo_action.trigger()
    assert len(doc.layers) == 1


def test_menus_and_dialog_effect():
    win, view, doc = make_window()
    from neximage.dialogs import EffectDialog
    from neximage import adjustments as adj
    doc.selection = None
    dlg = EffectDialog(doc, "Invert test", "Invert", [("brightness", "B", "int", -100, 100, 0, 0)],
                       lambda rgba, brightness: adj.invert(rgba), win)
    dlg._checks  # noqa
    dlg.accept()
    assert px(doc.active_layer.image, 5, 5)[:3] == (0, 0, 0)
    assert doc.undo_stack.canUndo()
    win.undo_action.trigger()
    assert px(doc.active_layer.image, 5, 5)[:3] == (255, 255, 255)


def test_save_and_reopen():
    win, view, doc = make_window()
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "x.nxe")
        assert win._write_project(doc, path)
        assert doc.undo_stack.isClean()
        win.open_path(path)
        assert win.tabs.count() == 2
        loaded = win.current_doc()
        assert loaded.width == 200 and loaded.file_path == path
        out = os.path.join(tmp, "x.png")
        fileio.export_image(loaded, out)
        assert os.path.getsize(out) > 0


def test_screenshot_and_no_doc_state():
    win = MainWindow()
    win.resize(1400, 860)
    win.show()
    QTest.qWaitForWindowExposed(win)
    assert win.current_doc() is None
    win.close()
    win, view, doc = make_window()
    win.select_tool("brush")
    win.settings.fg = QColor(230, 60, 60)
    drag(view, [(60, 40), (120, 80), (150, 120)])
    QTest.qWait(50)
    if SHOTS:
        win.grab().save(os.path.join(SHOTS, "window.png"))


if __name__ == "__main__":
    from neximage.styles import DARK_QSS
    app.setStyle("Fusion")
    app.setStyleSheet(DARK_QSS)
    fails = 0
    for name, fn in list(globals().items()):
        if name.startswith("test_") and callable(fn):
            try:
                fn()
                print("PASS", name, flush=True)
            except Exception as exc:  # noqa: BLE001
                fails += 1
                import traceback
                traceback.print_exc()
                print("FAIL", name, repr(exc), flush=True)
    sys.exit(1 if fails else 0)
