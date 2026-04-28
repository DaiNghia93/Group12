#include "ctrl_eqip_parser.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char *TAG = "ctrl_eqip_parser";

#define UART_PORT_NUM UART_NUM_0
#define UART_BAUD_RATE 115200
#define UART_BUF_SIZE 256

#define FRAME_START 0xAA
#define FRAME_END 0x55
#define MSG_TARGET 0x20

// Biến lưu trữ nội bộ
static CeAiData g_latest_data = {0, 0.0f, 0.0f, 0.0f};
static TickType_t g_last_recv_tick = 0; // Thời điểm nhận gói tin cuối cùng
#define DATA_TIMEOUT_MS 5000            // Hết hạn sau 5 giây không nhận được gì

// Hàm tính CRC
static uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x07;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

// Hàm giải mã Payload
static void handle_target_info(const uint8_t *payload) {
  CeAiData new_data;
  new_data.person_count = payload[0];

  uint32_t d_raw = ((uint32_t)payload[1] << 24) | ((uint32_t)payload[2] << 16) |
                   ((uint32_t)payload[3] << 8) | (uint32_t)payload[4];
  memcpy(&new_data.distance_m, &d_raw, 4);

  uint32_t x_raw = ((uint32_t)payload[5] << 24) | ((uint32_t)payload[6] << 16) |
                   ((uint32_t)payload[7] << 8) | (uint32_t)payload[8];
  memcpy(&new_data.target_x, &x_raw, 4);

  uint32_t y_raw = ((uint32_t)payload[9] << 24) |
                   ((uint32_t)payload[10] << 16) |
                   ((uint32_t)payload[11] << 8) | (uint32_t)payload[12];
  memcpy(&new_data.target_y, &y_raw, 4);

  // Cập nhật dữ liệu toàn cục
  g_latest_data = new_data;
  g_last_recv_tick = xTaskGetTickCount(); // Đánh dấu thời điểm nhận

  if (g_latest_data.person_count > 0) {
    ESP_LOGD(TAG, "Cap nhat: %d nguoi, khoang cach: %.2fm",
             g_latest_data.person_count, g_latest_data.distance_m);
  }
}

// Task đọc UART ngầm
static void uart_rx_task(void *arg) {
  uint8_t rx_buf[64];
  size_t rx_idx = 0;
  bool reading_frame = false;
  uint8_t byte_buf[1];

  while (true) {
    int len = uart_read_bytes(UART_PORT_NUM, byte_buf, 1, pdMS_TO_TICKS(20));
    if (len <= 0)
      continue;

    uint8_t b = byte_buf[0];

    if (!reading_frame) {
      if (b == FRAME_START) {
        reading_frame = true;
        rx_idx = 0;
      }
      continue;
    }

    if (rx_idx < sizeof(rx_buf)) {
      rx_buf[rx_idx++] = b;
    } else {
      reading_frame = false;
      continue;
    }

    if (rx_idx < 3)
      continue;

    uint8_t type = rx_buf[0];
    uint8_t payload_len = rx_buf[1];
    size_t expected_total = (size_t)payload_len + 4;

    if (rx_idx < expected_total)
      continue;

    uint8_t received_crc = rx_buf[payload_len + 2];
    uint8_t received_end = rx_buf[payload_len + 3];

    if (received_end == FRAME_END) {
      uint8_t computed_crc = crc8(rx_buf, payload_len + 2);
      if (computed_crc == received_crc) {
        if (type == MSG_TARGET) {
          handle_target_info(rx_buf + 2);
        }
      } else {
        ESP_LOGW(TAG, "Sai CRC!");
      }
    }
    reading_frame = false;
  }
}

// Khai báo APIs

void ce_parser_init(void) {
  uart_config_t uart_config = {};
  uart_config.baud_rate = UART_BAUD_RATE;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.source_clk = UART_SCLK_DEFAULT;

  uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
  uart_param_config(UART_PORT_NUM, &uart_config);

  xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 10, NULL);
  ESP_LOGI(TAG, "Parser da khoi dong. Dang nghe tai UART_NUM_0...");
}

CeAiData ce_parser_get_data(void) {
  // Nếu quá 3 giây không nhận gói tin mới -> tự động reset về 0
  if (g_last_recv_tick > 0) {
    TickType_t elapsed = xTaskGetTickCount() - g_last_recv_tick;
    if (elapsed >= pdMS_TO_TICKS(DATA_TIMEOUT_MS)) {
      g_latest_data.person_count = 0;
      g_latest_data.distance_m = 0.0f;
      g_latest_data.target_x = 0.0f;
      g_latest_data.target_y = 0.0f;
    }
  }
  return g_latest_data;
}
