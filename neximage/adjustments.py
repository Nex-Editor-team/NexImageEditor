"""Pixel algorithms for adjustments and filters.

Every function takes an (H, W, 4) uint8 non-premultiplied RGBA array and
returns a new array of the same shape. Alpha is preserved unless noted.
"""
import numpy as np
import cv2


def _rgb_f(rgba):
    return rgba[..., :3].astype(np.float32)


def _with_rgb(rgba, rgb):
    out = rgba.copy()
    out[..., :3] = np.clip(rgb + 0.5, 0, 255).astype(np.uint8)
    return out


def brightness_contrast(rgba, brightness: int = 0, contrast: int = 0):
    """brightness, contrast in -100..100 (Photoshop-like)."""
    rgb = _rgb_f(rgba)
    c = float(contrast)
    factor = 259.0 * (c + 255.0) / (255.0 * (259.0 - c)) if c < 259 else 10.0
    rgb = factor * (rgb - 128.0) + 128.0 + brightness * 2.55
    return _with_rgb(rgba, rgb)


def hue_saturation(rgba, hue: int = 0, saturation: int = 0, lightness: int = 0):
    """hue in -180..180 degrees, saturation/lightness in -100..100."""
    rgb = rgba[..., :3].copy()
    hsv = cv2.cvtColor(rgb, cv2.COLOR_RGB2HSV).astype(np.float32)  # H in 0..180
    hsv[..., 0] = (hsv[..., 0] + hue / 2.0) % 180.0
    hsv[..., 1] = np.clip(hsv[..., 1] * (1.0 + saturation / 100.0), 0, 255)
    hsv[..., 2] = np.clip(hsv[..., 2] + lightness * 2.55, 0, 255)
    back = cv2.cvtColor(hsv.astype(np.uint8), cv2.COLOR_HSV2RGB)
    out = rgba.copy()
    out[..., :3] = back
    return out


def levels(rgba, black: int = 0, white: int = 255, gamma: float = 1.0):
    """Input black/white points (0..255) and midtone gamma (0.1..10)."""
    black = int(min(black, white - 1))
    white = max(int(white), black + 1)
    x = np.arange(256, dtype=np.float32)
    lut_f = np.clip((x - black) / float(white - black), 0, 1) ** (1.0 / max(gamma, 0.01))
    lut = np.clip(lut_f * 255.0 + 0.5, 0, 255).astype(np.uint8)
    out = rgba.copy()
    for ch in range(3):
        out[..., ch] = cv2.LUT(np.ascontiguousarray(rgba[..., ch]), lut)
    return out


def threshold(rgba, level: int = 128):
    lum = _luma(rgba)
    v = np.where(lum >= level, 255, 0).astype(np.uint8)
    out = rgba.copy()
    out[..., :3] = v[..., None]
    return out


def invert(rgba):
    out = rgba.copy()
    out[..., :3] = 255 - rgba[..., :3]
    return out


def _luma(rgba):
    rgb = rgba[..., :3].astype(np.float32)
    return rgb[..., 0] * 0.299 + rgb[..., 1] * 0.587 + rgb[..., 2] * 0.114


def desaturate(rgba):
    lum = _luma(rgba)
    return _with_rgb(rgba, np.repeat(lum[..., None], 3, axis=2))


def gaussian_blur(rgba, radius: float = 2.0):
    """Blur in premultiplied space so transparent pixels don't leak dark fringes."""
    if radius <= 0:
        return rgba.copy()
    a = rgba[..., 3].astype(np.float32) / 255.0
    pr = rgba[..., :3].astype(np.float32) * a[..., None]
    sigma = max(radius, 0.1)
    pr_b = cv2.GaussianBlur(pr, (0, 0), sigma)
    a_b = cv2.GaussianBlur(a, (0, 0), sigma)
    with np.errstate(divide="ignore", invalid="ignore"):
        rgb = np.where(a_b[..., None] > 1e-6, pr_b / np.maximum(a_b[..., None], 1e-6), 0)
    out = np.empty_like(rgba)
    out[..., :3] = np.clip(rgb + 0.5, 0, 255).astype(np.uint8)
    out[..., 3] = np.clip(a_b * 255.0 + 0.5, 0, 255).astype(np.uint8)
    return out


def unsharp_mask(rgba, amount: float = 50.0, radius: float = 2.0, threshold: int = 0):
    """amount in percent (0..500), radius in px, threshold in levels (0..255)."""
    rgb = rgba[..., :3].astype(np.float32)
    blur = cv2.GaussianBlur(rgb, (0, 0), max(radius, 0.1))
    diff = rgb - blur
    if threshold > 0:
        diff = np.where(np.abs(diff) >= threshold, diff, 0.0)
    return _with_rgb(rgba, rgb + diff * (amount / 100.0))


def median_blur(rgba, radius: int = 1):
    k = int(radius) * 2 + 1
    if k <= 1:
        return rgba.copy()
    return cv2.medianBlur(np.ascontiguousarray(rgba), k)


def add_noise(rgba, amount: float = 20.0, monochrome: bool = False, seed: int | None = None):
    rng = np.random.default_rng(seed)
    sigma = amount * 2.55 * 0.5
    if monochrome:
        n = rng.normal(0, sigma, size=rgba.shape[:2])[..., None].repeat(3, axis=2)
    else:
        n = rng.normal(0, sigma, size=rgba.shape[:2] + (3,))
    rgb = rgba[..., :3].astype(np.float32) + n
    return _with_rgb(rgba, rgb)
