"""Pixel helpers that bridge QImage (Qt) and NumPy arrays (algorithms).

Layer pixels are stored as QImage.Format_ARGB32_Premultiplied (what QPainter
expects). Numeric algorithms work on non-premultiplied RGBA uint8 arrays with
shape (H, W, 4).
"""
import numpy as np
import cv2
from PySide6.QtGui import QImage, QColor

PREMULT = QImage.Format.Format_ARGB32_Premultiplied


def new_image(w: int, h: int, color: QColor | None = None) -> QImage:
    img = QImage(max(1, int(w)), max(1, int(h)), PREMULT)
    img.fill(color if color is not None else QColor(0, 0, 0, 0))
    return img


def to_premult(img: QImage) -> QImage:
    return img.convertToFormat(PREMULT)


def to_rgba(img: QImage) -> np.ndarray:
    """QImage -> (H, W, 4) uint8 non-premultiplied RGBA copy."""
    im = img.convertToFormat(QImage.Format.Format_RGBA8888)
    h, w = im.height(), im.width()
    bpl = im.bytesPerLine()
    buf = np.frombuffer(im.constBits(), dtype=np.uint8, count=im.sizeInBytes())
    return buf.reshape(h, bpl)[:, : w * 4].reshape(h, w, 4).copy()


def from_rgba(arr: np.ndarray) -> QImage:
    """(H, W, 4) uint8 RGBA -> premultiplied QImage (owns its data)."""
    arr = np.ascontiguousarray(arr, dtype=np.uint8)
    h, w = arr.shape[:2]
    im = QImage(arr.data, w, h, w * 4, QImage.Format.Format_RGBA8888)
    return im.convertToFormat(PREMULT)  # convertToFormat always copies


def mask_window(mask: np.ndarray, x: int, y: int, w: int, h: int) -> np.ndarray:
    """Extract the part of a canvas-sized mask that overlaps a (x, y, w, h) box.

    Returns an (h, w) uint8 array aligned with the box (zeros outside canvas).
    """
    out = np.zeros((h, w), np.uint8)
    H, W = mask.shape
    x0, y0 = max(0, x), max(0, y)
    x1, y1 = min(W, x + w), min(H, y + h)
    if x1 <= x0 or y1 <= y0:
        return out
    out[y0 - y:y1 - y, x0 - x:x1 - x] = mask[y0:y1, x0:x1]
    return out


def flood_mask(rgba: np.ndarray, seed: tuple[int, int], tol: int,
               contiguous: bool) -> np.ndarray:
    """Boolean mask of pixels similar to the seed pixel (per-channel tolerance).

    contiguous=True keeps only the 4-connected region containing the seed
    (classic bucket/wand behaviour); False selects every similar pixel.
    """
    x, y = int(seed[0]), int(seed[1])
    ref = rgba[y, x].astype(np.int16)
    similar = np.abs(rgba.astype(np.int16) - ref).max(axis=2) <= tol
    if not contiguous:
        return similar
    n, labels = cv2.connectedComponents(similar.astype(np.uint8), connectivity=4)
    return labels == labels[y, x]


def blend_with_mask(old: np.ndarray, new: np.ndarray, mask: np.ndarray | None) -> np.ndarray:
    """Replace `old` by `new` where mask (0..255) is set, smoothly at edges."""
    if mask is None:
        return new
    m = (mask.astype(np.float32) / 255.0)[..., None]
    out = old.astype(np.float32) * (1.0 - m) + new.astype(np.float32) * m
    return np.clip(out + 0.5, 0, 255).astype(np.uint8)
