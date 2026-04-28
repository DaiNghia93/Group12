#include "servo_track.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "soc/gpio_num.h"
#include <cmath>

static const char *TAG = "servo_track";

// ==== CẤU HÌNH PHẦN CỨNG ====
#define SERVO_GPIO GPIO_NUM_18

// Dùng Timer 2 cho Servo để cách ly hoàn toàn với Timer 0 (Quạt)
#define SERVO_LEDC_TIMER LEDC_TIMER_2
#define SERVO_LEDC_MODE LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_CHANNEL LEDC_CHANNEL_2
#define SERVO_LEDC_FREQ 50
#define SERVO_LEDC_RES LEDC_TIMER_14_BIT
#define SERVO_DUTY_MAX 16383

// Thông số SG90: Pulse width 0.5ms (0°) đến 2.5ms (180°) trong chu kỳ 20ms
// Duty tương ứng ở 14-bit resolution:
//   0°   -> 0.5ms/20ms  * 16383 = ~409
//   180° -> 2.5ms/20ms  * 16383 = ~2047
#define SERVO_DUTY_MIN 409        // Tương ứng 0°
#define SERVO_DUTY_MAX_ANGLE 2047 // Tương ứng 180°

// ==== BIẾN TRẠNG THÁI ====
static float ema_x = (float)FRAME_WIDTH / 2.0f; // Khởi tạo ở giữa khung hình
static float current_angle = 90.0f;             // Góc hiện tại của servo

// ==== HÀM NỘI BỘ ====

// Chuyển đổi góc (0-180) sang giá trị duty LEDC
static uint32_t angle_to_duty(float angle) {
  // Nội suy tuyến tính: 0° -> DUTY_MIN, 180° -> DUTY_MAX_ANGLE
  return (uint32_t)(SERVO_DUTY_MIN +
                    (angle / 180.0f) * (SERVO_DUTY_MAX_ANGLE - SERVO_DUTY_MIN));
}

// Đặt góc servo thực tế
static void servo_set_angle(float angle) {
  uint32_t duty = angle_to_duty(angle);
  ledc_set_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, duty);
  ledc_update_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL);
  current_angle = angle;
}

// ==== API CÔNG KHAI ====

void servo_track_init(void) {
  // 1. Cấu hình LEDC Timer 1 cho Servo (50Hz)
  ledc_timer_config_t timer_cfg = {};
  timer_cfg.speed_mode = SERVO_LEDC_MODE;
  timer_cfg.duty_resolution = SERVO_LEDC_RES;
  timer_cfg.timer_num = SERVO_LEDC_TIMER;
  timer_cfg.freq_hz = SERVO_LEDC_FREQ;
  timer_cfg.clk_cfg = LEDC_AUTO_CLK;
  ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

  // 2. Cấu hình LEDC Channel 1
  ledc_channel_config_t ch_cfg = {};
  ch_cfg.speed_mode = SERVO_LEDC_MODE;
  ch_cfg.channel = SERVO_LEDC_CHANNEL;
  ch_cfg.timer_sel = SERVO_LEDC_TIMER;
  ch_cfg.gpio_num = SERVO_GPIO;
  ch_cfg.duty = 0;
  ch_cfg.hpoint = 0;
  ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));

  // 3. Đưa servo về giữa (90°) ngay khi khởi động
  servo_set_angle(90.0f);

  ESP_LOGI(TAG, "Servo SG90 da khoi tao. GPIO: %d, Vi tri: 90 do (giua).",
           SERVO_GPIO);
}

void servo_track_update(float target_x) {
  // 1. Áp dụng bộ lọc EMA để làm mịn tọa độ X
  ema_x = EMA_ALPHA_SERVO * target_x + (1.0f - EMA_ALPHA_SERVO) * ema_x;

  // 2. Mapping: Pixel X (0-640) -> Góc (0-180°)
  float new_angle = (ema_x / (float)FRAME_WIDTH) * 180.0f;

  // Clamp an toàn cho servo SG90
  if (new_angle < 0.0f)
    new_angle = 0.0f;
  if (new_angle > 180.0f)
    new_angle = 180.0f;

  // 3. Kiểm tra Hysteresis: Chỉ xoay khi thay đổi đủ lớn
  float delta = fabsf(new_angle - current_angle);
  if (delta < HYSTERESIS_DEG) {
    return; // Sai lệch quá nhỏ, bỏ qua để tránh giật
  }

  // 4. Cập nhật servo
  servo_set_angle(new_angle);

  ESP_LOGD(TAG, "X=%.0f -> Goc=%.1f do (delta=%.1f)", ema_x, new_angle, delta);
}

void servo_track_center(void) {
  // Đưa về giữa 90° và reset bộ lọc EMA
  ema_x = (float)FRAME_WIDTH / 2.0f;
  servo_set_angle(90.0f);
  ESP_LOGI(TAG, "Servo ve giua (90 do).");
}
