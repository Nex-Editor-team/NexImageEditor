# NexImageEditor

Trình chỉnh sửa ảnh desktop dùng C++17, Qt 6 và OpenCV. Hỗ trợ layer ảnh,
chữ, hình khối, biến đổi, chỉnh màu và xuất ảnh PNG/JPEG/WebP.

## Build và kiểm thử (Ubuntu/Debian)

```sh
sudo apt install build-essential cmake qt6-base-dev libopencv-dev qt6-image-formats-plugins
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/NexImageEditor
```

WebP cần plugin định dạng ảnh của Qt. Đây là ứng dụng desktop, không phải web app.

## Các chức năng nền tảng mới

- **Bảo vệ thay đổi chưa lưu:** dấu `*` trên tiêu đề; New, Open và Quit hỏi
  Save / Discard / Cancel. Hủy hộp thoại lưu hoặc lưu lỗi không làm mất dự án.
- **Lưu an toàn:** ghi dự án qua `QSaveFile`, chỉ thay thế tệp đích khi ghi thành công.
- **Undo/Redo:** thay đổi thứ tự và ẩn/hiện layer được ghi vào lịch sử;
  canvas, danh sách layer và bảng thuộc tính được cập nhật khi hoàn tác.
  Hoàn tác về mốc lưu sẽ xóa dấu chưa lưu. Mỗi lần kéo/biến đổi layer
  là một bước riêng; chỉnh màu bằng bàn phím và con lăn cũng được ghi lịch sử.
- **Mở dự án an toàn:** kiểm tra định dạng, phiên bản, kích thước canvas và
  dữ liệu ảnh. Đọc lỗi không thay đổi dự án đích; mở lại không cộng dồn layer.
- **Canvas → Canvas Size…** (`Ctrl+Alt+C`): đổi kích thước từ 1–20.000 px
  mỗi chiều, có tùy chọn giữ nội dung ở giữa; hỗ trợ Undo/Redo.
  Không co giãn ảnh và không xóa dữ liệu layer nằm ngoài canvas.
  Tắt giữ nội dung ở giữa để giữ nguyên tọa độ layer (neo góc trên trái).
  Canvas lớn có thể cần nhiều RAM khi xuất ảnh.
- **Layer → Duplicate Layer** (`Ctrl+D`) và **Delete Layer** (`Delete`).

Các phím tắt có sẵn: `Ctrl+N/O/S` tạo/mở/lưu dự án, `Ctrl+Shift+S` lưu thành,
`Ctrl+I` nhập ảnh, `Ctrl+E` xuất ảnh, `Ctrl+Z` hoàn tác,
`Ctrl+C/V` sao chép/dán layer trong ứng dụng,
`Ctrl+Shift+C/V` sao chép/dán ảnh qua clipboard hệ điều hành.

## Kiểm tra thủ công giao diện

1. Thêm layer, lưu dự án; đổi thứ tự hoặc ẩn layer. Kiểm tra dấu `*` và Undo/Redo.
2. Đổi canvas với cả hai chế độ neo; Undo/Redo rồi lưu/mở lại.
3. Khi có thay đổi, thử New/Open/Quit với Save, Discard và Cancel.
   Chọn Save rồi hủy chọn đường dẫn: dự án phải còn nguyên.
4. Thêm layer, chọn layer rồi Undo thao tác thêm. Các bảng thuộc tính phải
   bỏ chọn, không giữ con trỏ tới layer đã bị loại khỏi dự án.
5. Xuất ảnh sau khi đổi canvas; kiểm tra kích thước ảnh đầu ra.

## Kiểm thử tự động và CI

- `EditorCoreTests`: thứ tự/visibility layer, kích thước canvas và tọa độ,
  mốc lưu, các bước biến đổi độc lập, lưu/mở ảnh/chữ/hình, mở lặp lại,
  dữ liệu dự án lỗi và đường dẫn lưu không hợp lệ.
- `EditorUiTests`: đồng bộ danh sách layer, bỏ chọn layer khi Undo thao tác thêm,
  dấu chưa lưu, chỉnh thanh trượt không dùng thao tác kéo, hủy đóng ứng dụng,
  hủy Save As và chọn Discard.
- CTest chạy Qt bằng nền tảng `offscreen`, không cần màn hình.
- `.github/workflows/build.yml` build và chạy hai bộ test trên Ubuntu với Qt6
  và OpenCV mỗi lần push/PR, hoặc chạy thủ công bằng `workflow_dispatch`.

CI chỉ thực thi sau khi thay đổi được đưa lên GitHub. Để kiểm tra trên máy cá nhân,
sử dụng các lệnh build/test ở đầu tài liệu.

## Build AppImage trên GitHub Actions

Workflow **Build AppImage** (`.github/workflows/appimage.yml`) tự chạy khi push/PR,
hoặc chọn **Actions → Build AppImage → Run workflow** để chạy thủ công sau khi
workflow có trên nhánh mặc định. Workflow build Release, đóng gói Qt6/OpenCV
bằng linuxdeploy + plugin Qt, rồi kiểm tra khởi động bản đã đóng gói. Bộ test
core/UI vẫn chạy riêng trong workflow **Build and test**; artifact AppImage
không có nghĩa là toàn bộ regression test đã đạt.

Tải artifact **NexImageEditor-AppImage-x86_64-…** ở cuối trang workflow run thành công.
Giải nén ZIP để lấy `NexImageEditor-<commit>-x86_64.AppImage` và `SHA256SUMS`:

```sh
sha256sum -c SHA256SUMS
chmod +x NexImageEditor-*-x86_64.AppImage
./NexImageEditor-*-x86_64.AppImage
```

- Bản hiện tại dành cho Linux **x86_64**, build trên Ubuntu 24.04
  (glibc 2.39); không cam kết chạy trên các distro cũ hơn.
- Đóng gói thư viện Qt/OpenCV và plugin ảnh WebP; không cần cài các gói dev trên
  máy người dùng. Các thư viện hệ thống/driver đồ họa vẫn phụ thuộc máy chạy.
- Nếu máy không có FUSE, dùng `./NexImageEditor-*-x86_64.AppImage --appimage-extract`
  rồi chạy `./squashfs-root/AppRun`.
- Artifacts giữ 14 ngày; workflow không tự phát hành GitHub Release.
- Phiên bản công cụ đóng gói được cố định trong workflow. Khi nâng linuxdeploy,
  cập nhật cả checksum SHA-256 tương ứng.


## Kéo thả ảnh vào canvas

Kéo một hoặc nhiều tệp PNG, JPEG, WebP, BMP hoặc TIFF từ trình quản lý tệp
vào vùng canvas. Mỗi ảnh tạo một layer riêng, tâm ảnh nằm tại vị trí thả
(có tính zoom/pan); ảnh cuối cùng được chọn để chỉnh sửa ngay. Nhiều ảnh
trong cùng một lần thả được hoàn tác bằng một lần `Ctrl+Z`, và có thể Redo.
Tệp gốc không bị di chuyển hay sửa đổi.

Chỉ nhận tệp ảnh cục bộ, không tải URL từ Internet. Thư mục và định dạng
không hỗ trợ bị bỏ qua. Nếu ảnh lỗi hoặc thiếu plugin giải mã, ứng dụng
báo tên tệp không đọc được; các ảnh hợp lệ trong lần thả vẫn được nhập.
`DragDropTests` kiểm tra thả nhiều ảnh, vị trí theo zoom, Undo/Redo, từ chối
URL/thư mục/văn bản và trường hợp tệp ảnh hỏng.
