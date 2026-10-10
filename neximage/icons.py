"""Vector-drawn UI icons (no external icon files needed)."""
from PySide6.QtCore import Qt, QRectF, QPointF
from PySide6.QtGui import QIcon, QPixmap, QPainter, QPen, QColor, QPainterPath, QBrush, QLinearGradient

ICON_COLOR = QColor(205, 205, 205)


def _pen(color, width=1.6, dash=False):
    pen = QPen(color, width)
    pen.setCapStyle(Qt.PenCapStyle.RoundCap)
    pen.setJoinStyle(Qt.PenJoinStyle.RoundJoin)
    if dash:
        pen.setStyle(Qt.PenStyle.DashLine)
    return pen


def _draw(name: str, p: QPainter, c: QColor):
    p.setRenderHint(QPainter.RenderHint.Antialiasing)
    p.setPen(_pen(c))
    p.setBrush(Qt.BrushStyle.NoBrush)
    if name == "move":
        p.drawLine(QPointF(12, 3), QPointF(12, 21))
        p.drawLine(QPointF(3, 12), QPointF(21, 12))
        for tip, a, b in [((12, 3), (9, 6), (15, 6)), ((12, 21), (9, 18), (15, 18)),
                          ((3, 12), (6, 9), (6, 15)), ((21, 12), (18, 9), (18, 15))]:
            path = QPainterPath(QPointF(*tip))
            path.lineTo(QPointF(*a))
            path.lineTo(QPointF(*b))
            path.closeSubpath()
            p.setBrush(QBrush(c))
            p.drawPath(path)
            p.setBrush(Qt.BrushStyle.NoBrush)
    elif name == "marquee":
        p.setPen(_pen(c, 1.4, dash=True))
        p.drawRect(QRectF(4, 5, 16, 14))
    elif name == "ellipse_marquee":
        p.setPen(_pen(c, 1.4, dash=True))
        p.drawEllipse(QRectF(4, 5, 16, 14))
    elif name == "lasso":
        path = QPainterPath(QPointF(5, 17))
        path.cubicTo(QPointF(0, 6), QPointF(14, 0), QPointF(18, 7))
        path.cubicTo(QPointF(22, 13), QPointF(12, 16), QPointF(11, 13))
        p.drawPath(path)
        p.drawLine(QPointF(5, 17), QPointF(7, 22))
    elif name == "wand":
        p.drawLine(QPointF(4, 20), QPointF(15, 9))
        p.drawLine(QPointF(15, 9), QPointF(17, 11))
        for cx, cy in [(18, 4), (21, 9), (14, 3)]:
            p.drawLine(QPointF(cx - 2, cy), QPointF(cx + 2, cy))
            p.drawLine(QPointF(cx, cy - 2), QPointF(cx, cy + 2))
    elif name == "crop":
        p.drawLine(QPointF(6, 1), QPointF(6, 18))
        p.drawLine(QPointF(6, 18), QPointF(23, 18))
        p.drawLine(QPointF(18, 6), QPointF(1, 6))
        p.drawLine(QPointF(18, 6), QPointF(18, 23))
    elif name == "eyedropper":
        p.setBrush(QBrush(c))
        p.drawEllipse(QRectF(15, 2, 6, 6))
        p.setBrush(Qt.BrushStyle.NoBrush)
        p.setPen(_pen(c, 2.6))
        p.drawLine(QPointF(14, 8), QPointF(5, 17))
        p.setPen(_pen(c))
        p.drawLine(QPointF(5, 17), QPointF(3, 21))
    elif name == "brush":
        p.setPen(_pen(c, 2))
        p.drawLine(QPointF(20, 3), QPointF(10, 13))
        p.setPen(_pen(c))
        path = QPainterPath(QPointF(10, 13))
        path.cubicTo(QPointF(6, 13), QPointF(3, 15), QPointF(3, 20))
        path.cubicTo(QPointF(9, 21), QPointF(13, 19), QPointF(12, 15))
        p.drawPath(path)
    elif name == "eraser":
        p.setBrush(QBrush(c))
        path = QPainterPath()
        path.addRoundedRect(QRectF(3, 8, 18, 10), 2, 2)
        p.save()
        p.translate(12, 13)
        p.rotate(-35)
        p.translate(-12, -13)
        p.drawPath(path)
        p.restore()
        p.setBrush(Qt.BrushStyle.NoBrush)
        p.drawLine(QPointF(9, 21), QPointF(22, 21))
    elif name == "bucket":
        p.setBrush(QBrush(c))
        path = QPainterPath(QPointF(4, 11))
        path.lineTo(QPointF(11, 3))
        path.lineTo(QPointF(18, 10))
        path.lineTo(QPointF(11, 17))
        path.closeSubpath()
        p.drawPath(path)
        p.setBrush(Qt.BrushStyle.NoBrush)
        p.drawArc(QRectF(15, 9, 5, 5), 0, -180 * 16)
        p.setBrush(QBrush(c))
        d = QPainterPath(QPointF(20, 16))
        d.quadTo(QPointF(23, 20), QPointF(20, 21))
        d.quadTo(QPointF(17, 20), QPointF(20, 16))
        p.drawPath(d)
    elif name == "gradient":
        grad = QLinearGradient(QPointF(3, 0), QPointF(21, 0))
        grad.setColorAt(0, QColor(40, 40, 40))
        grad.setColorAt(1, c)
        p.setPen(_pen(c, 1.2))
        p.setBrush(QBrush(grad))
        p.drawRect(QRectF(3, 6, 18, 12))
    elif name == "text":
        p.setPen(_pen(c, 1.8))
        p.drawLine(QPointF(5, 5), QPointF(19, 5))
        p.drawLine(QPointF(12, 5), QPointF(12, 20))
        p.drawLine(QPointF(8, 20), QPointF(16, 20))
    elif name == "shape":
        p.drawRect(QRectF(3, 3, 11, 11))
        p.drawEllipse(QRectF(10, 10, 11, 11))
    elif name == "hand":
        path = QPainterPath(QPointF(7, 14))
        path.lineTo(QPointF(7, 4))
        p.drawPath(path)
        for x in (10, 13, 16):
            p.drawLine(QPointF(x, 9), QPointF(x, 3))
        p.drawLine(QPointF(7, 14), QPointF(5, 11))
        p.drawLine(QPointF(16, 9), QPointF(18, 11))
        p.drawLine(QPointF(18, 11), QPointF(19, 16))
        p.drawArc(QRectF(7, 13, 12, 9), 180 * 16, 180 * 16)
    elif name == "zoom":
        p.drawEllipse(QRectF(3, 3, 13, 13))
        p.drawLine(QPointF(13, 13), QPointF(21, 21))
        p.drawLine(QPointF(6, 9), QPointF(13, 9))
        p.drawLine(QPointF(9.5, 5.5), QPointF(9.5, 12.5))
    elif name == "transform":
        p.setPen(_pen(c, 1.3, dash=True))
        p.drawRect(QRectF(4, 6, 16, 12))
        p.setPen(_pen(c))
        p.setBrush(QBrush(c))
        for x, y in [(4, 6), (20, 6), (4, 18), (20, 18)]:
            p.drawRect(QRectF(x - 2, y - 2, 4, 4))
    elif name == "eye":
        path = QPainterPath(QPointF(2, 12))
        path.quadTo(QPointF(12, 2), QPointF(22, 12))
        path.quadTo(QPointF(12, 22), QPointF(2, 12))
        p.drawPath(path)
        p.setBrush(QBrush(c))
        p.drawEllipse(QRectF(9.5, 9.5, 5, 5))
    elif name == "lock":
        p.drawRect(QRectF(5, 11, 14, 10))
        p.drawArc(QRectF(8, 4, 8, 12), 0, 180 * 16)
    elif name == "new":
        p.drawRect(QRectF(4, 4, 16, 16))
        p.drawLine(QPointF(12, 8), QPointF(12, 16))
        p.drawLine(QPointF(8, 12), QPointF(16, 12))
    elif name == "duplicate":
        p.drawRect(QRectF(3, 3, 12, 12))
        p.drawRect(QRectF(9, 9, 12, 12))
    elif name == "trash":
        p.drawLine(QPointF(4, 6), QPointF(20, 6))
        p.drawRect(QRectF(6, 7, 12, 14))
        p.drawLine(QPointF(10, 3), QPointF(14, 3))
    elif name == "up":
        p.drawLine(QPointF(12, 19), QPointF(12, 5))
        p.drawLine(QPointF(6, 11), QPointF(12, 5))
        p.drawLine(QPointF(18, 11), QPointF(12, 5))
    elif name == "down":
        p.drawLine(QPointF(12, 5), QPointF(12, 19))
        p.drawLine(QPointF(6, 13), QPointF(12, 19))
        p.drawLine(QPointF(18, 13), QPointF(12, 19))
    elif name == "merge":
        p.drawRect(QRectF(3, 3, 8, 8))
        p.drawRect(QRectF(13, 13, 8, 8))
        p.drawLine(QPointF(12, 7), QPointF(12, 17))
        p.drawLine(QPointF(9, 14), QPointF(12, 17))
        p.drawLine(QPointF(15, 14), QPointF(12, 17))
    elif name == "fx":
        p.drawEllipse(QRectF(4, 4, 16, 16))
    else:
        p.drawEllipse(QRectF(5, 5, 14, 14))


def make_icon(name: str, color: QColor = ICON_COLOR, size: int = 24) -> QIcon:
    pix = QPixmap(size, size)
    pix.fill(Qt.GlobalColor.transparent)
    p = QPainter(pix)
    p.scale(size / 24.0, size / 24.0)
    _draw(name, p, color)
    p.end()
    return QIcon(pix)
