#include "fan_speed.h"
#include "dht.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include "freertos/semphr.h"

static const char *TAG = "fan_speed";
static SemaphoreHandle_t dht_mutex;

// ==== CẤU HÌNH L298N ====
#define L298N_ENA_GPIO GPIO_NUM_4 // Chân xuất PWM
#define L298N_IN1_GPIO GPIO_NUM_5 // Chân hướng 1
#define L298N_IN2_GPIO GPIO_NUM_6 // Chân hướng 2

// ==== CẤU HÌNH DHT22 ====
#define DHT_GPIO GPIO_NUM_8      // Chân OUT của DHT22
#define DHT_TYPE DHT_TYPE_AM2301 // DHT22 trong esp-idf-lib dùng AM2301

// ==== CẤU HÌNH LEDC PWM ====
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_10_BIT // 10-bit resolution (0-1023)
#define LEDC_DUTY_MAX 1023
#define LEDC_FREQUENCY 25000 // 25 kHz (vượt ngưỡng nghe)

// ==== EMA FILTER ====
#define EMA_ALPHA                                                              \
  0.25f // Hệ số làm mịn (càng nhỏ càng mượt nhưng phản ứng chậm)
static float ema_dist = 3.0f;
static float ema_temp = 25.0f;



// ==== BIẾN NỘI BỘ CHO DHT22 ====
static float g_dht_temp = 25.0f; // Nhiệt độ mặc định khi chưa đọc được
static float g_dht_humidity = 0.0f;

// Task chạy ngầm, đọc DHT22 mỗi 2 giây (theo yêu cầu datasheet)
static void dht_read_task(void *arg) {
  float temp = 0.0f;
  float hum = 0.0f;
  // Chờ 1 giây sau khi boot cho cảm biến ổn định
  vTaskDelay(pdMS_TO_TICKS(1000));

  while (true) {
    esp_err_t ret = dht_read_float_data(DHT_TYPE, DHT_GPIO, &hum, &temp);

    if (ret == ESP_OK) {
      if (dht_mutex) {
          xSemaphoreTake(dht_mutex, portMAX_DELAY);
          g_dht_temp = temp;
          g_dht_humidity = hum;
          xSemaphoreGive(dht_mutex);
      } else {
          // fallback nếu mutex lỗi
          g_dht_temp = temp;
          g_dht_humidity = hum;
      }

      ESP_LOGI(TAG, "[DHT22] Nhiet do: %.1f°C | Do am: %.1f%%", temp, hum);
    } else {
      ESP_LOGW(TAG, "[DHT22] Doc that bai! (Loi: %d). Dung gia tri cu: %.1f°C",
               ret, g_dht_temp);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
float fan_get_temperature(void) {
    float temp;

    if (dht_mutex) {
        xSemaphoreTake(dht_mutex, portMAX_DELAY);
        temp = g_dht_temp;
        xSemaphoreGive(dht_mutex);
    } else {
        temp = g_dht_temp;
    }

    return temp;
}

void fan_speed_init(void) {
  // 1. Cấu hình 2 chân Direction (IN1, IN2)
  dht_mutex = xSemaphoreCreateMutex();

  if (dht_mutex == NULL) {
    ESP_LOGE(TAG, "Tao mutex THAT BAI!");
  }
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (1ULL << L298N_IN1_GPIO) | (1ULL << L298N_IN2_GPIO);
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);

  // Mặc định cho quay tới (IN1 = High, IN2 = Low)
  gpio_set_level(L298N_IN1_GPIO, 1);
  gpio_set_level(L298N_IN2_GPIO, 0);
// delay nhỏ cho driver LEDC ổn định

  // 2. Cấu hình LEDC Timer cho chân ENA
  ledc_timer_config_t ledc_timer = {};
  ledc_timer.speed_mode = LEDC_MODE;
  ledc_timer.duty_resolution = LEDC_DUTY_RES;
  ledc_timer.timer_num = LEDC_TIMER;
  ledc_timer.freq_hz = LEDC_FREQUENCY;
  ledc_timer.clk_cfg = LEDC_AUTO_CLK;
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  // 3. Cấu hình LEDC Channel
  ledc_channel_config_t ledc_channel = {};
  ledc_channel.speed_mode = LEDC_MODE;
  ledc_channel.channel = LEDC_CHANNEL;
  ledc_channel.timer_sel = LEDC_TIMER;
  ledc_channel.gpio_num = L298N_ENA_GPIO;
  ledc_channel.duty = 0;
  ledc_channel.hpoint = 0;
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  // 4. Khởi tạo Task đọc DHT22 chạy ngầm
  xTaskCreate(dht_read_task, "dht_read", 2048, NULL, 5, NULL);

  ESP_LOGI(TAG,
           "Khoi tao L298N + DHT22. PWM: GPIO%d, IN1: GPIO%d, IN2: GPIO%d, "
           "DHT: GPIO%d",
           L298N_ENA_GPIO, L298N_IN1_GPIO, L298N_IN2_GPIO, DHT_GPIO);
  
}

void fan_speed_update(float dist_m) {
  // 1. Lấy nhiệt độ mới nhất từ DHT22 (đọc ngầm bởi task)
  float temp_c;
  gpio_set_level(L298N_IN1_GPIO, 1);
  gpio_set_level(L298N_IN2_GPIO, 0);

  if (dht_mutex) {
      xSemaphoreTake(dht_mutex, portMAX_DELAY);
      temp_c = g_dht_temp;
      xSemaphoreGive(dht_mutex);
  } else {
      temp_c = g_dht_temp; // fallback
  }

  // 2. Áp dụng bộ lọc EMA
  ema_dist = (EMA_ALPHA * dist_m) + ((1.0f - EMA_ALPHA) * ema_dist);
  ema_temp = (EMA_ALPHA * temp_c) + ((1.0f - EMA_ALPHA) * ema_temp);

  // 3. Tính toán PWM theo công thức tuyến tính
  // PWM = Base + Kt(T - T_min) + Kd(D - D_min)
  float pwm_percent =
      PWM_BASE + K_T * (ema_temp - TEMP_MIN) + K_D * (ema_dist - DIST_MIN);

  // 4. Clamp giá trị phần trăm (50% -> 100%)
  pwm_percent = std::max(50.0f, std::min(pwm_percent, 100.0f));

  // 5. Nội suy sang dải 10-bit (0 - 1023)
  uint32_t duty = (uint32_t)((pwm_percent / 100.0f) * LEDC_DUTY_MAX);

  // 6. Xuất xung ra Motor
  ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
  ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

  ESP_LOGD(TAG, "D=%.2fm, T=%.1fC -> PWM: %.1f%% (Duty: %lu)", ema_dist,
           ema_temp, pwm_percent, duty);
}

void fan_speed_stop(void) {
  ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
  ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

  // QUAN TRỌNG
  gpio_set_level(L298N_IN1_GPIO, 0);
  gpio_set_level(L298N_IN2_GPIO, 0);
}
