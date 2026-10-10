"""The document view: renders the composite, handles zoom/pan and routes input to tools."""
from PySide6.QtCore import Qt, QPointF, QRectF, QTimer, Signal, QSizeF
from PySide6.QtGui import QPainter, QColor, QPen, QPixmap, QBrush, QPainterPath
from PySide6.QtWidgets import QWidget

from .document import Document
from .settings import Settings
from .tools import Ev, TOOL_CLASSES, TransformTool

MIN_ZOOM, MAX_ZOOM = 0.05, 32.0
WORKSPACE_BG = QColor(64, 64, 64)


def _checker_brush() -> QBrush:
    pix = QPixmap(16, 16)
    pix.fill(QColor(204, 204, 204))
    p = QPainter(pix)
    p.fillRect(0, 0, 8, 8, QColor(255, 255, 255))
    p.fillRect(8, 8, 8, 8, QColor(255, 255, 255))
    p.end()
    return QBrush(pix)


class CanvasView(QWidget):
    zoomChanged = Signal(float)
    cursorChanged = Signal(object)     # QPointF in document coords, or None
    toolChanged = Signal(str)
    messageRequested = Signal(str)

    def __init__(self, doc: Document, settings: Settings, parent=None):
        super().__init__(parent)
        self.doc = doc
        self.settings = settings
        self.zoom = 1.0
        self.offset = QPointF(0, 0)     # widget position of the document origin
        self._need_fit = True
        self._space = False
        self._panning = False
        self._pan_last = QPointF()
        self._checker = _checker_brush()
        self._ants_phase = 0.0
        self.tools = {cls.name: cls(self) for cls in TOOL_CLASSES}
        self.tools["transform"] = TransformTool(self)
        self._prev_tool = settings.tool if settings.tool in self.tools else "move"
        self.tool = self.tools[self._prev_tool]

        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        self.setMinimumSize(120, 120)
        doc.changed.connect(self.update)
        self._ants_timer = QTimer(self)
        self._ants_timer.timeout.connect(self._tick_ants)
        self._ants_timer.start(120)
        self.setCursor(self.tool.cursor)

    # ------------------------------------------------------------ view math
    def to_doc(self, p: QPointF) -> QPointF:
        return QPointF((p.x() - self.offset.x()) / self.zoom,
                       (p.y() - self.offset.y()) / self.zoom)

    def to_widget(self, p: QPointF) -> QPointF:
        return QPointF(p.x() * self.zoom + self.offset.x(), p.y() * self.zoom + self.offset.y())

    def fit(self):
        if self.width() < 10 or self.height() < 10:
            self._need_fit = True
            return
        z = min((self.width() - 60) / max(1, self.doc.width),
                (self.height() - 60) / max(1, self.doc.height))
        self.zoom = max(MIN_ZOOM, min(z, 1.0))  # fit down, never blow small images up
        self._center_doc()

    def _center_doc(self):
        w, h = self.doc.width * self.zoom, self.doc.height * self.zoom
        self.offset = QPointF((self.width() - w) / 2, (self.height() - h) / 2)
        self.zoomChanged.emit(self.zoom)
        self.update()

    def set_zoom(self, z: float, anchor: QPointF | None = None):
        z = max(MIN_ZOOM, min(MAX_ZOOM, z))
        if anchor is None:
            anchor = QPointF(self.width() / 2, self.height() / 2)
        doc_pt = self.to_doc(anchor)
        self.zoom = z
        self.offset = QPointF(anchor.x() - doc_pt.x() * z, anchor.y() - doc_pt.y() * z)
        self.zoomChanged.emit(self.zoom)
        self.update()

    def zoom_to_rect(self, r: QRectF):
        z = min(self.width() / max(1.0, r.width()), self.height() / max(1.0, r.height())) * 0.9
        self.zoom = max(MIN_ZOOM, min(MAX_ZOOM, z))
        c = r.center()
        self.offset = QPointF(self.width() / 2 - c.x() * self.zoom,
                              self.height() / 2 - c.y() * self.zoom)
        self.zoomChanged.emit(self.zoom)

    def message(self, text: str):
        self.messageRequested.emit(text)

    # ----------------------------------------------------------- tools
    def set_tool(self, name: str):
        if name not in self.tools:
            return
        if self.tool.name != name:
            self.tool.deactivate()
        if name != "transform":
            self._prev_tool = name
            self.settings.tool = name
        self.tool = self.tools[name]
        self.setCursor(self.tool.cursor)
        self.toolChanged.emit(name)
        self.message(self.tool.hint)
        self.update()

    def start_transform(self):
        t = self.tools["transform"]
        if t.begin():
            self.tool.deactivate()
            self.tool = t
            self.setCursor(t.cursor)
            self.toolChanged.emit("transform")
            self.message(t.hint)
            self.update()

    def _after_key(self):
        if self.tool.name == "transform" and not self.tool.active:
            self.set_tool(self._prev_tool)

    def _ev(self, e) -> Ev:
        return Ev(self.to_doc(e.position()), e.button(), e.modifiers())

    # ----------------------------------------------------------- paint
    def _tick_ants(self):
        if self.doc.selection is not None:
            self._ants_phase = (self._ants_phase + 1) % 16
            self.update()

    def paintEvent(self, _event):
        if self._need_fit:
            self._need_fit = False
            self.fit()
        p = QPainter(self)
        p.fillRect(self.rect(), WORKSPACE_BG)
        p.setRenderHint(QPainter.RenderHint.Antialiasing, False)
        W, H = self.doc.width, self.doc.height
        doc_rect = QRectF(self.offset.x(), self.offset.y(), W * self.zoom, H * self.zoom)
        p.fillRect(doc_rect.translated(2, 3), QColor(20, 20, 20, 140))
        p.fillRect(doc_rect, self._checker)
        p.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform, self.zoom < 1.0)
        p.drawImage(doc_rect, self.doc.composite())

        p.save()
        p.translate(self.offset)
        p.scale(self.zoom, self.zoom)
        p.setRenderHint(QPainter.RenderHint.Antialiasing, True)
        self.tool.paint(p, self.zoom)
        p.restore()

        sel = self.doc.selection
        if sel is not None and not sel.path.isEmpty():
            p.save()
            p.translate(self.offset)
            p.scale(self.zoom, self.zoom)
            p.setRenderHint(QPainter.RenderHint.Antialiasing, True)
            solid = QPen(QColor(0, 0, 0), 1)
            solid.setCosmetic(True)
            p.setPen(solid)
            p.setBrush(Qt.BrushStyle.NoBrush)
            p.drawPath(sel.path)
            dashed = QPen(QColor(255, 255, 255), 1)
            dashed.setCosmetic(True)
            dashed.setStyle(Qt.PenStyle.CustomDashLine)
            dashed.setDashPattern([4, 4])
            dashed.setDashOffset(self._ants_phase)
            p.setPen(dashed)
            p.drawPath(sel.path)
            p.restore()

        border = QPen(QColor(10, 10, 10), 1)
        p.setPen(border)
        p.setBrush(Qt.BrushStyle.NoBrush)
        p.drawRect(doc_rect.adjusted(-0.5, -0.5, 0.5, 0.5))
        p.end()

    # ----------------------------------------------------------- input
    def mousePressEvent(self, e):
        self.setFocus()
        hand = self.tool.name == "hand" and e.button() == Qt.MouseButton.LeftButton
        if e.button() == Qt.MouseButton.MiddleButton or self._space or hand:
            self._panning = True
            self._pan_last = e.position()
            self.setCursor(Qt.CursorShape.ClosedHandCursor)
            return
        self.tool.press(self._ev(e))
        self.update()

    def mouseMoveEvent(self, e):
        pos = e.position()
        self.cursorChanged.emit(self.to_doc(pos))
        if self._panning:
            self.offset += pos - self._pan_last
            self._pan_last = pos
            self.update()
            return
        ev = self._ev(e)
        if e.buttons() & Qt.MouseButton.LeftButton:
            self.tool.move(ev)
        else:
            self.tool.hover(ev)
        self.update()

    def mouseReleaseEvent(self, e):
        if self._panning:
            self._panning = False
            self.setCursor(Qt.CursorShape.OpenHandCursor if self._space else self.tool.cursor)
            return
        self.tool.release(self._ev(e))
        self.update()

    def leaveEvent(self, _e):
        self.cursorChanged.emit(None)

    def wheelEvent(self, e):
        dy = e.angleDelta().y()
        if dy == 0:
            return
        self.set_zoom(self.zoom * (1.12 ** (dy / 120.0)), e.position())

    def keyPressEvent(self, e):
        if e.key() == Qt.Key.Key_Space and not e.isAutoRepeat():
            self._space = True
            self.setCursor(Qt.CursorShape.OpenHandCursor)
            return
        if e.key() in (Qt.Key.Key_BracketLeft, Qt.Key.Key_BracketRight) and \
                self.tool.name in ("brush", "eraser"):
            step = 1.25 if e.key() == Qt.Key.Key_BracketRight else 0.8
            self.settings.brush_size = max(1, min(500, round(self.settings.brush_size * step)))
            self.update()
            return
        if self.tool.key(e.key(), e.modifiers()):
            self._after_key()
            self.update()
            return
        super().keyPressEvent(e)

    def keyReleaseEvent(self, e):
        if e.key() == Qt.Key.Key_Space and not e.isAutoRepeat():
            self._space = False
            self.setCursor(self.tool.cursor)
            return
        super().keyReleaseEvent(e)

    def resizeEvent(self, e):
        super().resizeEvent(e)
        if self._need_fit:
            self.fit()
