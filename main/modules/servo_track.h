#ifndef SERVO_TRACK_H
#define SERVO_TRACK_H

#ifdef __cplusplus
extern "C" {
#endif

// ==== HẰNG SỐ CẤU HÌNH ====
#define FRAME_WIDTH        640      // Chiều rộng khung hình AI (pixel)
#define HYSTERESIS_DEG     3.0f     // Ngưỡng trễ: chỉ xoay khi thay đổi > 3 độ
#define EMA_ALPHA_SERVO    0.15f    // Hệ số làm mịn cho tọa độ X

/**
 * @brief Khởi tạo Servo (SG90) trên GPIO 7.
 * Cấu hình LEDC Timer 1 ở 50Hz và đưa servo về vị trí giữa (90°).
 */
void servo_track_init(void);

/**
 * @brief Cập nhật góc servo theo tọa độ X của người gần nhất.
 * Áp dụng bộ lọc EMA + Hysteresis để chuyển động mượt mà.
 * @param target_x Tọa độ trung điểm X của Bounding Box (0 đến FRAME_WIDTH).
 */
void servo_track_update(float target_x);

/**
 * @brief Đưa servo về vị trí chính giữa (90°).
 * Dùng khi mất người hoặc Timeout.
 */
void servo_track_center(void);

#ifdef __cplusplus
}
#endif

#endif // SERVO_TRACK_H
