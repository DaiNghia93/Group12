#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

void wifi_manager_init(void);
void wifi_manager_task(void *pv);
bool wifi_is_connected(void);

// trạng thái điều khiển từ web
bool wifi_is_manual_mode(void);
int  wifi_get_speed_level(void);
bool wifi_get_fan_state(void);

#ifdef __cplusplus
}
#endif

#endif