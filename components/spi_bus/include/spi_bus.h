#pragma once
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"

typedef struct {
    spi_host_device_t host;
    spi_dma_chan_t dma;
} spi_bus_t;

esp_err_t spi_bus_init(spi_bus_t *bus,
                       spi_host_device_t host,
                       gpio_num_t mosi,
                       gpio_num_t miso,
                       gpio_num_t sclk);

esp_err_t spi_bus_deinit(spi_bus_t *bus);