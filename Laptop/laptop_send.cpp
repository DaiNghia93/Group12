// --- Laptop AI + Serial Send Example (Final Clean Version) ---
// Phien ban nay da loai bo hoan toan cac canh bao ve volatile va thu vien du thua.

#ifdef __clang__
#undef noreturn
#define noreturn
#endif

#include <stdio.h>
#include <time.h>
#include "../include/ctrl_eqip.h"

int main(int argc, char* argv[]) {
    printf("=== ctrl_eqip: Laptop AI + ESP32 Serial ===\n");

    const char* model_path = "models/Body-Detection-Model_640x384.onnx";
    const char* com_port   = "COM3";

    if (argc >= 2) com_port = argv[1];

    // Khởi tạo AI
    printf("[1/2] Dang khoi tao AI Pipeline...\n");
    CePipelineHandle pipeline = ce_pipeline_start(model_path, 640, 480);
    if (!pipeline) {
        fprintf(stderr, "[LOI] Khong the khoi tao Pipeline!\n");
        return -1;
    }

    // Kết nối ESP32
    printf("[2/2] Dang ket noi ESP32 tai %s...\n", com_port);
    CeControllerHandle controller = ce_controller_open(com_port);
    if (!controller) {
        fprintf(stderr, "[LOI] Khong mo duoc %s. Tiep tuc chay AI thoi...\n", com_port);
    } else {
        printf("[OK] Da ket noi ESP32.\n");
    }

    printf("\n>>> Dang chay... Nhan Ctrl+C de dung <<<\n\n");

    CeTrackingResult result;
    clock_t last_log_time = clock();
    int frames_processed = 0;
    int frames_sent = 0;

    while (1) {
        if (ce_pipeline_try_recv(pipeline, &result)) {
            frames_processed++;
            if (result.has_person) {
                printf("\r[AI] %d nguoi | Gan: %.2fm | Toa do: (%.0f, %.0f)     ",
                       (int)result.person_count, result.closest_distance_m,
                       result.primary_x, result.primary_y);
                fflush(stdout);

                if (controller) {
                    ce_controller_send_tracking(controller, &result);
                    frames_sent++;
                }
            }
        }

        // Tự động log thống kê 3 giây
        clock_t now = clock();
        if ((now - last_log_time) >= 3 * CLOCKS_PER_SEC) {
            float fs = (float)(now - last_log_time) / (float)CLOCKS_PER_SEC;
            float fps = (float)frames_processed / fs;

            printf("\n--- Thong ke %.1fs ---\n", fs);
            printf("  AI FPS     : %.1f\n", fps);
            printf("  Frame gui  : %d\n", frames_sent);
            printf("--------------------\n\n");

            frames_processed = 0;
            frames_sent = 0;
            last_log_time = now;
        }

        // Vong lap tre nhe nhang de khong ton CPU
        clock_t start_idle = clock();
        while (clock() - start_idle < 1);
    }

    if (controller) ce_controller_close(controller);
    ce_pipeline_stop(pipeline);
    return 0;
}
