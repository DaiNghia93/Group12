set(EXTRA_COMPONENT_DIRS ../esp-idf-lib/components)

idf_component_register(
    SRCS "main.cpp"
         "modules/ctrl_eqip_parser.cpp"
         "modules/fan_speed.cpp"
         "modules/servo_track.cpp"
         "modules/wifi_manager.cpp"
         "modules/oled_display.cpp"

    INCLUDE_DIRS "." "modules"

    REQUIRES 
        freertos
        esp_driver_uart
        esp_driver_gpio
        esp_driver_ledc
        log
        esp_wifi
        esp_http_server
        nvs_flash
        driver

)