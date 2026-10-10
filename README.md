# NexImageEditor

Trình chỉnh sửa ảnh nhẹ viết bằng **Qt6 + C++17**, theo kiến trúc:

```
NexImageEditor
├── GUI          (MainWindow, Canvas, LayersPanel, PropertiesPanel)
├── Image Engine (ImageBuffer, Transform, ColorAdjust, Filters, Blend)
├── Project      (Layer, TextLayer, ShapeLayer, Commands = Undo/Redo)
└── IO           (ImageIO: PNG/JPEG/WebP, ProjectIO: .nxproj)
```

## Build

Yêu cầu: CMake ≥ 3.16, Qt6 (Widgets/Gui/Core), OpenCV 4.x (core/imgproc/photo),
trình biên dịch hỗ trợ C++17.

```bash
sudo apt install qt6-base-dev libopencv-dev cmake build-essential
# (tuỳ chọn, để đọc/ghi WebP):
sudo apt install qt6-image-formats-plugins

cd NexImageEditor
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/NexImageEditor
```

### Lỗi "Could not find OpenCVConfig.cmake"

Nếu CMake báo không tìm thấy OpenCV dù đã `apt install libopencv-dev`, tìm
xem file config thật sự nằm ở đâu rồi chỉ thẳng CMake vào đó:

```bash
find / -name OpenCVConfig.cmake 2>/dev/null
cmake -B build -DOpenCV_DIR=<thư mục chứa file đó>
```

(Ví dụ trên Ubuntu thường là `/usr/lib/x86_64-linux-gnu/cmake/opencv4`.)
CMakeLists.txt đã tự dò các đường dẫn phổ biến trước khi báo lỗi, nhưng một
số distro/bản cài đặt custom có thể đặt file config ở chỗ khác.

## Tính năng chính

- **Layers**: raster (ảnh import), text (render từ font/màu), shape
  (rectangle/ellipse/line) — mỗi layer có opacity, blend mode (9 chế độ
  qua `QPainter::CompositionMode`), visible/lock, và **Free Transform**
  độc lập (không phá huỷ pixel gốc cho tới khi bạn "bake").

- **Transform / Free Transform** (`src/engine/Transform.h`, `src/gui/Canvas.cpp`):
  - Kéo 8 tay cầm quanh layer đang chọn để scale theo trục X/Y hoặc cả hai.
  - Kéo tay cầm tròn phía trên để **xoay tự do** quanh tâm layer
    (giữ Shift để snap 15°).
  - Kéo vào giữa khung để **di chuyển**.
  - Nhập số chính xác (X/Y, Scale X/Y, Rotation, Shear) ở Properties panel.
  - Flip ngang/dọc, xoay 90° CW/CCW, Reset Transform (menu **Transform**).
  - Mỗi thao tác transform đẩy một `TransformLayerCommand` vào
    `QUndoStack` → Undo/Redo đầy đủ (Ctrl+Z / Ctrl+Shift+Z), các bước
    kéo liên tiếp trên cùng layer được gộp lại (`mergeWith`).

- **Image Engine**: resize (nearest/bilinear), crop, rotate 90°,
  flip, brightness/contrast, saturation, hue shift, levels, grayscale,
  invert, sepia, box blur, sharpen (unsharp mask).

- **Shape layers (mở rộng)**: 9 loại — Rectangle, Rounded Rectangle,
  Ellipse, Triangle, Pentagon, Hexagon, Star, Arrow, Line. Mỗi shape
  hỗ trợ **fill mode Solid / Linear Gradient / Radial Gradient** —
  "mix" (pha trộn) giữa `fillColor` và `fillColor2`, chỉnh góc gradient
  (Linear) qua Properties panel; Rounded Rectangle có thêm corner
  radius riêng. Toàn bộ đều đẩy vào Undo/Redo như các thuộc tính khác.

- **Color Mix (tint)** — áp dụng cho MỌI loại layer (raster/text/shape):
  chọn một màu và % pha trộn (0-100%), pha trực tiếp vào từng pixel
  (giữ nguyên alpha gốc). Không phá huỷ pixel gốc — tự tính lại mỗi khi
  đổi màu/độ pha hoặc khi nội dung layer thay đổi (`applyTint()`).

- **Background Removal (xoá nền)** — cho layer ảnh (raster): chọn màu
  nền cần xoá + ngưỡng dung sai (tolerance), có feather mép mượt.
  Đây là thuật toán color-distance đơn giản (không AI/segmentation),
  phù hợp cho ảnh nền phẳng/một màu (ảnh chụp studio, screenshot...).

- **Export config** — dialog xuất ảnh cho phép chọn định dạng
  (PNG lossless / JPEG / WebP) và chỉnh thanh trượt Quality (chỉ bật
  với định dạng lossy).

- **Sharpen (OpenCV)** — tool tăng độ nét dùng OpenCV thật (`libopencv-dev`,
  modules core/imgproc/photo), thay cho bộ unsharp-mask tự viết bằng
  box-blur trước đây. Có trong Properties panel cho mọi layer có pixel:
  - **Unsharp Mask**: `result = src + amount * (src - GaussianBlur(src, radius))`,
    có Threshold để tránh làm nét vùng phẳng/nhiễu.
  - **Smart Sharpen**: thêm bước khử nhiễu giữ cạnh
    (`cv::edgePreservingFilter`) trước khi unsharp mask, cho ảnh mượt hơn.
  Thao tác là destructive (ghi đè pixel layer) và có Undo/Redo.

- **IO**: import ảnh (PNG/JPEG/WebP/BMP/TIFF) làm layer mới, export
  ảnh đã flatten; lưu/mở project dạng `.nxproj` (JSON, tự chứa toàn bộ
  pixel layer dạng base64 PNG — không phụ thuộc file ngoài).

## Giới hạn đã biết (do đây là bản "nhẹ")

- Chưa có brush/paint trực tiếp lên layer (chỉ import ảnh/text/shape).
- Chưa có layer mask, layer groups, hay adjustment layers không phá huỷ.
- `moveLayer` (đổi thứ tự layer) hiện chưa đẩy vào Undo stack.
- Hit-test layer trên canvas dùng bounding box, chưa test theo alpha
  pixel-perfect.
- WebP cần plugin `qtimageformats` cài riêng (xem trên).

Đây là một khung sườn đầy đủ, biên dịch được và chạy được — các phần
trên là những chỗ dễ mở rộng tiếp theo nhất nếu cần.
