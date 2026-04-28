#ifndef FAN_SPEED_H
#define FAN_SPEED_H

#ifdef __cplusplus
extern "C" {
#endif

// Hằng số tính toán PWM
#define PWM_BASE       50.0f   // Mức PWM cơ sở để chống kẹt motor (%)
#define K_T            4.0f    // Hệ số khuếch đại cho Nhiệt độ
#define K_D            15.0f    // Hệ số khuếch đại cho Khoảng cách
#define TEMP_MIN       25.0f   // Mức nhiệt độ cơ sở (Dưới mốc này tính là lạnh)
#define DIST_MIN       1.0f    // Khoảng cách cơ sở (1 mét)

/**
 * @brief Khởi tạo module điều khiển quạt L298N + cảm biến DHT22.
 * Cấu hình PWM (GPIO 4), chân Hướng (GPIO 5, 6), và tạo Task đọc DHT22 (GPIO 8).
 */
void fan_speed_init(void);

/**
 * @brief Cập nhật liên tục tốc độ quạt theo khoảng cách.
 * Nhiệt độ được đọc tự động từ DHT22 bên trong module.
 * @param dist_m Khoảng cách đo bằng mét (từ AI).
 */
void fan_speed_update(float dist_m);

/**
 * @brief Dừng quạt ngay lập tức (dùng khi Timeout mất người).
 */
void fan_speed_stop(void);

#ifdef __cplusplus
}
#endif

#endif // FAN_SPEED_H
