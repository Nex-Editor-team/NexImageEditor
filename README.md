# NexImageEditor

A layer-based raster image editor written in **C++17 with Qt 6**, laid out
like Photoshop: menu bar, options bar, toolbox on the left, document tabs,
Layers / Properties / History panels on the right, and a status bar.

## Features

- **Selections:** rectangle and ellipse marquee, lasso, magic wand (contiguous
  or global, adjustable tolerance); Shift adds, Alt subtracts; select all,
  deselect, invert, copy, paste as new layer.
- **Move:** moves the layer, or the selected pixels when a selection exists.
- **Painting:** brush (soft or hard), eraser, paint bucket, linear gradient,
  eyedropper. All painting is clipped to the selection.
- **Shapes and text:** rectangle and ellipse (filled or stroked), raster text.
- **Transform:** crop (Enter/Esc), free transform with move, scale and rotate
  (Enter/Esc), image size, canvas size, rotate, flip.
- **Adjustments:** brightness/contrast, hue/saturation, levels, tint, invert,
  grayscale.
- **Filters:** Gaussian blur, unsharp mask, median, add noise. Each has live
  preview in its dialog.
- **Layers:** add, duplicate, delete, merge down, reorder, flatten, visibility,
  lock, opacity, 12 blend modes, position.
- **Undo/redo** for every edit (History panel shows the list).
- **Files:** open and export PNG, JPEG, WebP, BMP, TIFF; save and reopen
  `.nxe` projects (layers stored as PNG inside a binary container).

### Not carried over from 1.x

The previous C++ version (1.x, OpenCV-based) had curves, presets, background
removal, OpenCV sharpen, and text/shape vector layers. These are not in 2.0.

## Keyboard shortcuts

| Key | Tool | Key | Tool |
|---|---|---|---|
| V | Move | B | Brush |
| M / Shift+M | Rect / ellipse marquee | E | Eraser |
| L | Lasso | G / Shift+G | Bucket / gradient |
| W | Magic wand | T | Text |
| C | Crop | U / Shift+U | Rect / ellipse shape |
| I | Eyedropper | Ctrl+T | Free transform |
| Z | Zoom | H or Space+drag | Hand (pan) |

X swaps foreground and background colours; D resets them. Ctrl+Z / Ctrl+Y
(or Ctrl+Shift+Z) undo and redo.

## Build

Requirements: CMake 3.16+, a C++17 compiler, Qt 6 (Core, Gui, Widgets).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/NexImageEditor
```

Run the tests:

```bash
cmake -S . -B build-test -DNEX_BUILD_TESTS=ON
cmake --build build-test --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build-test --output-on-failure
```

On Debian/Ubuntu install `qt6-base-dev` and `qt6-image-formats-plugins`
(the latter provides WebP and TIFF support).

## Packages

Pushing a `v*` tag runs `.github/workflows/build-cpp.yml`, which builds a
`.deb` and a `tar.gz` and attaches them to the GitHub release.

## License

See [LICENSE](LICENSE).
