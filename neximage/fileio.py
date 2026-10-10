"""Project (.nxe) and image file input/output."""
import json
import os
import zipfile

from PySide6.QtCore import QBuffer, QByteArray, QIODevice, Qt
from PySide6.QtGui import QImage, QColor

from . import imgutil as iu
from .document import Document, Layer

PROJECT_FORMAT = "nxe"
PROJECT_VERSION = 1
IMAGE_FILTER = "Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff *.gif)"
EXPORT_FORMATS = {
    ".png": "PNG", ".jpg": "JPEG", ".jpeg": "JPEG", ".webp": "WEBP",
    ".bmp": "BMP", ".tif": "TIFF", ".tiff": "TIFF",
}


def _png_bytes(img: QImage) -> bytes:
    ba = QByteArray()
    buf = QBuffer(ba)
    buf.open(QIODevice.OpenModeFlag.WriteOnly)
    img.save(buf, "PNG")
    buf.close()
    return bytes(ba)


def save_project(doc: Document, path: str) -> None:
    meta = {
        "format": PROJECT_FORMAT,
        "version": PROJECT_VERSION,
        "width": doc.width,
        "height": doc.height,
        "active": doc.active,
        "layers": [
            {
                "name": l.name, "x": l.x, "y": l.y, "visible": l.visible,
                "locked": l.locked, "opacity": l.opacity, "blend": l.blend,
                "background": l.background, "file": f"layers/{i:03d}.png",
            }
            for i, l in enumerate(doc.layers)
        ],
    }
    tmp = path + ".tmp"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("project.json", json.dumps(meta, indent=2))
        for i, layer in enumerate(doc.layers):
            z.writestr(f"layers/{i:03d}.png", _png_bytes(layer.image))
    os.replace(tmp, path)


def load_project(path: str) -> Document:
    with zipfile.ZipFile(path, "r") as z:
        meta = json.loads(z.read("project.json"))
        if meta.get("format") != PROJECT_FORMAT:
            raise ValueError("Not a NexImageEditor project")
        doc = Document(meta["width"], meta["height"], name=os.path.basename(path),
                       empty=True)
        for entry in meta["layers"]:
            img = QImage.fromData(z.read(entry["file"]), "PNG")
            layer = Layer(img, entry["name"], entry["x"], entry["y"],
                          background=entry.get("background", False))
            layer.visible = entry.get("visible", True)
            layer.locked = entry.get("locked", False)
            layer.opacity = float(entry.get("opacity", 1.0))
            layer.blend = entry.get("blend", "Normal")
            doc.layers.append(layer)
    doc.active = min(max(meta.get("active", 0), 0), len(doc.layers) - 1)
    doc.file_path = path
    return doc


def open_image_document(path: str) -> Document:
    img = QImage(path)
    if img.isNull():
        raise ValueError(f"Cannot read image: {path}")
    doc = Document(img.width(), img.height(), name=os.path.basename(path), empty=True)
    doc.layers.append(Layer(img, "Background", background=True))
    doc.active = 0
    return doc


def read_image(path: str) -> QImage:
    img = QImage(path)
    if img.isNull():
        raise ValueError(f"Cannot read image: {path}")
    return img


def export_image(doc: Document, path: str, quality: int = 92) -> None:
    ext = os.path.splitext(path)[1].lower()
    fmt = EXPORT_FORMATS.get(ext)
    if fmt is None:
        raise ValueError(f"Unsupported export format: {ext}")
    img = doc.composite()
    if fmt == "JPEG":
        # JPEG has no alpha: flatten over white first.
        flat = iu.new_image(doc.width, doc.height, QColor(255, 255, 255))
        from PySide6.QtGui import QPainter
        p = QPainter(flat)
        p.drawImage(0, 0, img)
        p.end()
        img = flat
    if not img.save(path, fmt, quality):
        raise IOError(f"Failed to write {path}")
