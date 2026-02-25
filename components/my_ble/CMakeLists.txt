idf_component_register(SRCS "my_ble.c"
                    INCLUDE_DIRS "include"
                    REQUIRES bt my_led esp-nimble-cpp)
