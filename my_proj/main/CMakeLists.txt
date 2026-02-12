idf_component_register(SRCS "my_proj.c"
                    INCLUDE_DIRS "."
                    REQUIRES my_led my_joystick esp_timer)
