# NexImageEditor

A Photoshop-style raster image editor (remake, Python + Qt 6).

## Run

```bash
python3 -m venv .venv && . .venv/bin/activate
pip install -r requirements.txt
python -m neximage [files...]
```

Tests: `python tests/test_core.py` and `python tests/test_gui.py` (run offscreen with
`QT_QPA_PLATFORM=offscreen`).

## Features

- **UI**: menu bar, tool options bar, vertical toolbox with FG/BG swatches, document tabs,
  Layers / History / Properties panels, zoom and cursor status bar, dark theme.
- **Layers**: add, duplicate, delete, merge down, flatten, move up/down, rename, visibility,
  lock, opacity, 12 blend modes, layer via copy (Ctrl+J).
- **Selection**: rectangular/elliptical marquee, lasso, magic wand (tolerance, contiguous),
  select all / deselect / inverse, add (Shift) and subtract (Alt).
- **Tools**: Move (also moves selected pixels), Crop, Brush, Eraser (size/hardness/opacity),
  Paint Bucket, Gradient, Eyedropper, Text (rasterized text layer), Rectangle/Ellipse shapes,
  Hand, Zoom, Free Transform (Ctrl+T, scale and move).
- **Adjustments**: Brightness/Contrast, Hue/Saturation, Levels, Threshold, Desaturate, Invert
  (live preview dialogs).
- **Filters**: Gaussian Blur, Unsharp Mask, Median, Add Noise.
- **Image**: Image Size (resample), Canvas Size (anchor), rotate 90°/180°, flip.
- **Editing**: unlimited undo/redo with a History panel, cut/copy/copy merged/paste,
  clear, fill with foreground color.
- **Files**: native project format `.nxe` (layers preserved), open PNG/JPEG/WebP/BMP/TIFF,
  place image as layer, export PNG/JPEG/WebP/BMP/TIFF.

## Limitations

- Text layers are rasterized (not re-editable).
- Layers are pixel buffers: moving pixels outside a layer grows its buffer.
- No channels, masks, smart objects, or pressure-sensitive brushes yet.

## Legacy C++ version

The original Qt6/C++ implementation remains under `src/` and `CMakeLists.txt`.
