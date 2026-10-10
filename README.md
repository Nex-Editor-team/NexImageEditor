# NexImageEditor

A layer-based raster image editor written in **C++17 with GTK 4**, laid out
like GIMP: menu bar, tool options bar, toolbox on the left, rulers and
document tabs over the canvas, Colour / Layers / History panels on the right,
and a status bar.

## Features

- **Selections:** rectangle and ellipse marquee, lasso, magic wand (contiguous
  or global, adjustable tolerance). The options bar sets New / Add / Subtract.
  Select all, deselect, invert.
- **Move:** moves the layer, or the selected pixels when a selection exists.
- **Painting:** round brush, eraser, bucket fill, linear gradient, eyedropper.
  All painting is clipped to the selection.
- **Shapes and text:** rectangle and ellipse (filled and/or stroked), raster text.
- **Transform:** crop (Enter applies, Esc cancels), free transform with move and
  scale (Enter applies, Esc cancels), canvas size, image size, rotate 90° / 180°,
  flip.
- **Adjustments:** invert, grayscale, brightness/contrast, hue/saturation,
  threshold, posterize.
- **Filters:** Gaussian blur, unsharp mask, median, add noise. Blur, sharpen,
  median and noise have live preview in their dialogs; the colour adjustments
  that take parameters also preview.
- **Layers:** add, duplicate, delete, merge down, reorder, flatten, visibility,
  opacity, 12 blend modes.
- **Colour:** HSV sliders and hex entry for the foreground and background colours.
- **Undo/redo** for every edit. The History panel lists the edits and jumps to any of them.
- **Copy / paste:** copies the active layer (selected pixels only, when a
  selection exists) and pastes it as a new layer. The clipboard is inside the
  app only.
- **Files:** open and export PNG, JPEG, WebP, BMP, TIFF. Save and reopen `.nxe`
  projects (layers stored as PNG inside a binary container).

### Not carried over from 1.x (OpenCV build)

The 1.x C++ version had curves, presets, background removal, OpenCV sharpen, and
text/shape vector layers, and a histogram. None of these are in this version.

### Not carried over from the Qt 2.x build

The GTK rewrite does not include some Qt 2.x features: layer lock, soft
(feathered) brush, Shift/Alt modifiers for selection (use the options-bar mode
instead), levels and tint adjustments, rotation in free transform, and the
X / D / Ctrl+Y / Shift+letter shortcuts. Copy and paste also use an in-app
clipboard, not the system clipboard.

## Keyboard shortcuts

| Key | Tool | Key | Tool |
|---|---|---|---|
| M | Rectangle marquee | B | Brush |
| L | Lasso | E | Eraser |
| W | Magic wand | G | Bucket fill |
| V | Move | T | Text |
| C | Crop | U | Rectangle shape |
| I | Eyedropper | Z | Zoom |
| H or hold Space | Hand (pan) | Middle-drag | Pan at any time |

Ellipse marquee, gradient, ellipse shape and free transform have no single key
here; pick them from the toolbox (free transform is also Ctrl+T).

Ctrl+Z undoes, Ctrl+Shift+Z redoes. Ctrl+N new, Ctrl+O open, Ctrl+S save,
Ctrl+Shift+S save as, Ctrl+Shift+E export, Ctrl+W close. Ctrl+A / Ctrl+D /
Ctrl+I select all / deselect / invert. Ctrl+0 fit, Ctrl+1 100%, Ctrl+plus and
Ctrl+minus zoom. Ctrl+scroll zooms at the pointer.

## Build

Requirements: CMake 3.16+, a C++17 compiler, pkg-config, and GTK 4 development
files (gtkmm-4.0), cairo, pangocairo, gdk-pixbuf and glib.

```bash
sudo apt-get install build-essential cmake pkg-config libgtkmm-4.0-dev \
  libcairo2-dev libpango1.0-dev libgdk-pixbuf-2.0-dev libglib2.0-dev webp-pixbuf-loader
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/NexImageEditor
```

Run the core tests (they do not need a display):

```bash
cmake -S . -B build-test -DNEX_BUILD_TESTS=ON
cmake --build build-test --parallel
ctest --test-dir build-test --output-on-failure
```

`webp-pixbuf-loader` provides WebP support; the other formats use the gdk-pixbuf
loaders that ship with GTK.

## Packages

Pushing a `v*` tag runs `.github/workflows/build-cpp.yml`, which builds a
`.deb` and a `tar.gz` and attaches them to the GitHub release.

## License

See [LICENSE](LICENSE).
