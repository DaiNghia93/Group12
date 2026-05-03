# Smart Fan - ESP32

##  Giới thiệu
Dự án quạt thông minh sử dụng ESP32. Hệ thống có khả năng tự động phát hiện người, bật quạt và điều chỉnh hướng gió theo vị trí.

##  Chức năng chính
- Phát hiện người và tính khoảng cách bằng AI
- Tự động bật/tắt quạt
- Điều khiển hướng quạt bằng servo
- Hiển thị trạng thái kết nối wifi, nhiệt độ, số người, chế độ lên OLED
- Kết nối WiFi và điều khiển qua web

##  Phần cứng sử dụng
- ESP32
- Cảm biến DHT22
- AI Tracking
- OLED SSD1306
- Servo motor
- Quạt
- L298N

##  Cấu trúc code
- `main/main.cpp`: chương trình chính
- `main/modules/`: các module chức năng
  - `fan_speed.*`: điều khiển tốc độ quạt
  - `servo_track.*`: điều khiển hướng quạt
  - `wifi_manager.*`: kết nối WiFi
  - `oled_display.*`: hiển thị OLED
  - `dht.*`: đọc cảm biến nhiệt độ

##  Cách chạy
1. Mở project bằng ESP-IDF
2. Build và flash:
