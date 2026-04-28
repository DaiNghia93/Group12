#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "modules/ctrl_eqip_parser.h"
#include "modules/fan_speed.h"
#include "modules/servo_track.h"

static const char *TAG = "smart_fan";

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "====================================");
  ESP_LOGI(TAG, " SMART FAN (FULL SYSTEM) START      ");
  ESP_LOGI(TAG, "====================================");

  // Khởi tạo các module
  ce_parser_init();
  fan_speed_init();
  servo_track_init();

  while (true) {
    // Lấy dữ liệu AI mới nhất (từ Laptop gửi qua Serial/WiFi)
    CeAiData ai_data = ce_parser_get_data();

    if (ai_data.person_count > 0) {
      // Có người -> Cập nhật tốc độ quạt và xoay servo bám theo
      // Tốc độ quạt thay đổi dựa trên khoảng cách (distance_m)
      fan_speed_update(ai_data.distance_m);
      
      // Servo xoay dựa trên vị trí X của đối tượng (target_x)
      servo_track_update(ai_data.target_x);
      
      ESP_LOGD(TAG, "Tracking: Dist=%.2fm, X=%.0f", ai_data.distance_m, ai_data.target_x);
    } else {
      // Không có người hoặc mất kết nối -> Dừng quạt và đưa servo về giữa
      fan_speed_stop();
      servo_track_center();
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // Nhịp quét 50ms (20fps) cực kỳ mượt
  }
}
