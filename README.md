# NexImageEditor

NexImageEditor là ứng dụng chỉnh sửa ảnh **desktop C++ / Qt 6**. Giao diện tối gồm canvas, thanh công cụ, bảng thuộc tính/chỉnh ảnh và danh sách layer. Khi khởi động, ứng dụng mở một bố cục mẫu có thể chỉnh sửa; dùng **File → Open Image** để mở ảnh của bạn.

## Build

Cần CMake, trình biên dịch C++17, Qt 6 Widgets và OpenCV (core, imgproc, photo). Trên Debian/Ubuntu:

```sh
sudo apt install cmake g++ qt6-base-dev libopencv-dev qt6-image-formats-plugins
cmake -S . -B build
cmake --build build -j
./build/NexImageEditor
```

Qt image formats plugins cần thiết cho một số định dạng, bao gồm WebP.

## Sử dụng

- Tạo project, mở ảnh theo kích thước gốc hoặc import ảnh vào layer. Lưu/mở project `.nxproj`; xuất ảnh PNG, JPEG hoặc WebP.
- Thêm layer ảnh, chữ và hình; chọn layer để chỉnh nội dung, màu, độ mờ và blend mode. Có thể ẩn, nhân bản, xóa, sắp xếp, sao chép và dán layer.
- Kéo layer hoặc các tay nắm để di chuyển, thay đổi kích thước và xoay. Giữ Shift khi kéo để giữ tỉ lệ hoặc xoay theo bước. Chọn công cụ bàn tay để di chuyển góc nhìn; cuộn chuột để zoom.
- Bảng **Adjustments** có histogram, thông số ánh sáng/màu, preset và curves; **Properties** chứa transform, chữ/hình, trộn màu, xóa nền và sharpen.
- **Image → Crop Canvas** cắt canvas bằng tọa độ pixel, không phá hủy layer và có thể undo. Các thao tác undo/redo dùng Ctrl+Z / Ctrl+Shift+Z.
- Ứng dụng hỏi trước khi đóng hoặc thay project có thay đổi chưa lưu. Hãy lưu project `.nxproj` để giữ các layer; xuất ảnh sẽ gộp các layer thành ảnh phẳng.

Ảnh mẫu *The Great Wave off Kanagawa* của Katsushika Hokusai được lấy từ [StoryMapJS-for-teaching/Hokusai-Great-Wave](https://github.com/StoryMapJS-for-teaching/Hokusai-Great-Wave).
