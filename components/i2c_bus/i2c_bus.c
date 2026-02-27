#include "i2c_bus.h"

esp_err_t i2c_bus_init(i2c_bus_t *bus,
                       i2c_port_t port,
                       gpio_num_t sda,
                       gpio_num_t scl)
{
    if (!bus) return ESP_ERR_INVALID_ARG;

    i2c_master_bus_config_t bus_config = {
        .i2c_port = port,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &bus->handle);
    if (err != ESP_OK) {
        return err;
    }

    bus->port = port;
    return ESP_OK;
}