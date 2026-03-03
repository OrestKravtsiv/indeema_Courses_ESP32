#include "spi_bus.h"

esp_err_t spi_bus_init(spi_bus_t *bus,
                       spi_host_device_t host,
                       gpio_num_t mosi,
                       gpio_num_t miso,
                       gpio_num_t sclk,
                       spi_dma_chan_t dma)
{
    if (!bus) return ESP_ERR_INVALID_ARG;

    spi_bus_config_t cfg = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    esp_err_t err = spi_bus_initialize(host, &cfg, dma);
    if (err != ESP_OK) return err;

    bus->host = host;
    bus->dma  = dma;
    return ESP_OK;
}

esp_err_t spi_bus_deinit(spi_bus_t *bus)
{
    if (!bus) return ESP_ERR_INVALID_ARG;
    return spi_bus_free(bus->host);
}