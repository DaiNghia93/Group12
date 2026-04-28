#ifndef CTRL_EQIP_PARSER_H
#define CTRL_EQIP_PARSER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Struct chứa dữ liệu AI nhận được từ Laptop
typedef struct {
  uint8_t person_count;
  float distance_m;
  float target_x;
  float target_y;
} CeAiData;

/**
 * Khởi tạo UART và chạy Task lắng nghe dữ liệu chạy ngầm.
 * Mặc định dùng UART_NUM_0, Baudrate 115200.
 */
void ce_parser_init(void);

/**
 * Lấy dữ liệu AI mới nhất bất cứ lúc nào.
 */
CeAiData ce_parser_get_data(void);

#ifdef __cplusplus
}
#endif

#endif // CTRL_EQIP_PARSER_H
