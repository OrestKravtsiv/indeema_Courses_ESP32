idf_component_register(SRCS "my_proj.c"
                    INCLUDE_DIRS "."
                    REQUIRES my_led my_joystick my_wifi my_mqtt my_ble i2c_bus my_bmp280 my_aht20 spi_bus my_lsm6ds3 my_sg92r my_step driver)
