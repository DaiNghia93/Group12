#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"

#include "modules/oled_display.h"
#include "modules/ctrl_eqip_parser.h"
#include "modules/fan_speed.h"
#include "modules/servo_track.h"
#include "modules/wifi_manager.h"

#define BUTTON_GPIO GPIO_NUM_20

static const char *TAG = "smart_fan";

// ===== GLOBAL =====
QueueHandle_t ai_queue;
TimerHandle_t no_person_timer;
TaskHandle_t control_task_handle = NULL;

volatile bool g_emergency_stop = false;

// ================== TIMER ==================
void no_person_timeout(TimerHandle_t xTimer) {
    ESP_LOGW(TAG, "No person -> FAN OFF");
    fan_speed_stop();
    servo_track_center();
}

// ================== ISR BUTTON ==================
static void IRAM_ATTR button_isr_handler(void* arg) {

    static uint32_t last_time = 0;
    static int last_level = 1;

    int level = gpio_get_level(BUTTON_GPIO);
    uint32_t now = xTaskGetTickCountFromISR();

    if ((now - last_time) < pdMS_TO_TICKS(200)) return;

    if (last_level == 1 && level == 0) {

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        vTaskNotifyGiveFromISR(control_task_handle, &xHigherPriorityTaskWoken);

        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }

        last_time = now;
    }

    last_level = level;
}

// ================== FAN LEVEL ==================
void fan_speed_set_level(int level) {

    float fake_distance = 0;

    switch(level) {
        case 1: fake_distance = 0.5; break;
        case 2: fake_distance = 1.0; break;
        case 3: fake_distance = 1.5; break;
        case 4: fake_distance = 2.0; break;
        case 5: fake_distance = 2.5; break;
        case 6: fake_distance = 3.0; break;
        default:
            fan_speed_stop();
            return;
    }

    fan_speed_update(fake_distance);
}

// ================== AI TASK ==================
void ai_forward_task(void *pv) {
    while (1) {
        if (!g_emergency_stop) {
            CeAiData data = ce_parser_get_data();
            xQueueOverwrite(ai_queue, &data);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ================== CONTROL TASK ==================
void control_task(void *pv) {

    CeAiData ai_data;

    while (1) {

        // ===== BUTTON =====
        // pdFALSE = không clear ngay, tránh miss nếu ISR fire lúc task bận
        uint32_t notif = ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(50));
        if (notif > 0) {
            // Clear toàn bộ count tích lũy -> chỉ toggle 1 lần dù nhấn nhanh
            ulTaskNotifyValueClear(control_task_handle, UINT32_MAX);

            g_emergency_stop = !g_emergency_stop;
            ESP_LOGW(TAG, "STATE: %s", g_emergency_stop ? "STOP" : "RUN");

            if (g_emergency_stop) {
                fan_speed_stop();
                servo_track_center();
            }
        }

        // ===== STOP MODE =====
        if (g_emergency_stop) {
            continue;
        }

        // ===== MANUAL MODE =====
        bool manual = wifi_is_manual_mode();
        if (manual) {
            int level = wifi_get_speed_level();
            bool on   = wifi_get_fan_state();

            if (!on) fan_speed_stop();
            else     fan_speed_set_level(level);

            servo_track_center();
            continue;
        }

        // ===== AUTO MODE =====
        if (xQueueReceive(ai_queue, &ai_data, pdMS_TO_TICKS(60))) {
            if (ai_data.person_count > 0) {
                xTimerStop(no_person_timer, 0);
                fan_speed_update(ai_data.distance_m);
                servo_track_update(ai_data.target_x);
            } else {
                xTimerStart(no_person_timer, 0);
            }
        }
    }
}

// ================== MAIN ==================
extern "C" void app_main(void) {

    ESP_LOGI(TAG, "SMART FAN START");

    // ===== BUTTON CONFIG =====
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);

    // ===== INIT MODULE =====
    ce_parser_init();
    fan_speed_init();
    servo_track_init();
    oled_display_init();
    wifi_manager_init();

    // ===== QUEUE =====
    ai_queue = xQueueCreate(1, sizeof(CeAiData));

    // ===== TIMER =====
    no_person_timer = xTimerCreate(
        "no_person",
        pdMS_TO_TICKS(180000),
        pdFALSE,
        NULL,
        no_person_timeout
    );

    // ===== TASK =====
    xTaskCreate(ai_forward_task, "ai_forward", 2048, NULL, 4, NULL);
    xTaskCreate(control_task, "control", 4096, NULL, 5, &control_task_handle);
    xTaskCreate(oled_display_task, "oled", 4096, NULL, 2, NULL);
    xTaskCreate(wifi_manager_task, "wifi", 4096, NULL, 3, NULL);

    // ===== ISR =====
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
}