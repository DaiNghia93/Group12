#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo I2C + OLED SSD1306 128x64.
 *        Gọi trước khi tạo task.
 */
void oled_display_init(void);

/**
 * @brief FreeRTOS Task — gọi bằng xTaskCreate().
 *        Tự động cập nhật màn hình mỗi 500ms.
 *        Lấy dữ liệu từ wifi_manager, fan_speed, ctrl_eqip_parser.
 */
void oled_display_task(void *pv);

#ifdef __cplusplus
}
#endif

#endif // OLED_DISPLAY_H
