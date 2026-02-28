#pragma once
#include "driver/i2c_master.h"

typedef struct {
    i2c_master_bus_handle_t handle;
    i2c_port_t port;
} i2c_bus_t;


esp_err_t i2c_bus_init(i2c_bus_t *bus,
                       i2c_port_t port,
                       gpio_num_t sda,
                       gpio_num_t scl);