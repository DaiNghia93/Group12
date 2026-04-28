idf_component_register(SRCS "main.cpp" 
                            "modules/ctrl_eqip_parser.cpp" 
                            "modules/fan_speed.cpp"
                            "modules/servo_track.cpp"
                    INCLUDE_DIRS "." "modules"
                    REQUIRES freertos esp_driver_uart esp_driver_gpio esp_driver_ledc log)
