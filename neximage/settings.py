"""Global tool settings shared by all documents (brush, colors, tool choice)."""
from PySide6.QtCore import QObject, Signal
from PySide6.QtGui import QColor


class Settings(QObject):
    colorsChanged = Signal()
    toolChanged = Signal(str)

    def __init__(self):
        super().__init__()
        self._fg = QColor(0, 0, 0)
        self._bg = QColor(255, 255, 255)
        self._tool = "move"
        self.brush_size = 24
        self.brush_hardness = 100   # %
        self.brush_opacity = 100    # %
        self.tolerance = 32         # 0..255
        self.contiguous = True
        self.marquee_shape = "rect"  # rect | ellipse
        self.shape_kind = "rect"     # rect | ellipse
        self.font_family = "Sans Serif"
        self.font_size = 48
        self.font_bold = False

    @property
    def fg(self) -> QColor:
        return QColor(self._fg)

    @fg.setter
    def fg(self, c: QColor):
        self._fg = QColor(c)
        self.colorsChanged.emit()

    @property
    def bg(self) -> QColor:
        return QColor(self._bg)

    @bg.setter
    def bg(self, c: QColor):
        self._bg = QColor(c)
        self.colorsChanged.emit()

    def swap_colors(self):
        self._fg, self._bg = self._bg, self._fg
        self.colorsChanged.emit()

    def reset_colors(self):
        self._fg, self._bg = QColor(0, 0, 0), QColor(255, 255, 255)
        self.colorsChanged.emit()

    @property
    def tool(self) -> str:
        return self._tool

    @tool.setter
    def tool(self, name: str):
        if name != self._tool:
            self._tool = name
            self.toolChanged.emit(name)
