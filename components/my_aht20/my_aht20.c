#include "my_aht20.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "AHT20";


esp_err_t aht20_init(aht20_t *dev, i2c_bus_t *bus, uint8_t addr)
{
    esp_err_t err;
    
    // Configure I2C device
    i2c_device_config_t cfg = {
        .device_address = addr,
        .scl_speed_hz = 400000,
    };

    dev->bus = bus;
    
    // Add device to I2C bus
    err = i2c_master_bus_add_device(bus->handle, &cfg, &dev->handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device: %d", err);
        return err;
    }
    
    // Wait 40ms after power-on (per datasheet)
    vTaskDelay(pdMS_TO_TICKS(AHT20_POWER_ON_DELAY));
    
    // Check if sensor is calibrated
    bool calibrated = false;
    err = aht20_is_calibrated(dev, &calibrated);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read calibration status");
        return err;
    }
    
    // If not calibrated, send initialization command
    if (!calibrated) {
        ESP_LOGI(TAG, "Sensor not calibrated, initializing...");
        
        uint8_t init_cmd[3] = {AHT20_CMD_INIT, 0x08, 0x00};
        err = i2c_master_transmit(dev->handle, init_cmd, sizeof(init_cmd), 1000);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Init command failed");
            return err;
        }
        
        // Wait 10ms after init
        vTaskDelay(pdMS_TO_TICKS(AHT20_INIT_DELAY));
    }
    
    ESP_LOGI(TAG, "AHT20 initialized successfully");
    return ESP_OK;
}

esp_err_t aht20_read_status(aht20_t *dev, uint8_t *status)
{
    uint8_t cmd = AHT20_CMD_STATUS;
    return i2c_master_transmit_receive(dev->handle, &cmd, 1, status, 1, 1000);
}


esp_err_t aht20_is_calibrated(aht20_t *dev, bool *calibrated)
{
    uint8_t status = 0;
    esp_err_t err = aht20_read_status(dev, &status);
    
    if (err == ESP_OK) {
        *calibrated = (status & AHT20_STATUS_CAL) != 0;
    }
    
    return err;
}


esp_err_t aht20_trigger_measurement(aht20_t *dev)
{
    uint8_t cmd[3] = {AHT20_CMD_TRIGGER, 0x33, 0x00};
    return i2c_master_transmit(dev->handle, cmd, sizeof(cmd), 1000);
}


esp_err_t aht20_read_raw(aht20_t *dev)
{
    esp_err_t err;
    
    // Trigger measurement
    err = aht20_trigger_measurement(dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Trigger failed");
        return err;
    }
    
    // Wait for measurement to complete
    vTaskDelay(pdMS_TO_TICKS(AHT20_MEASURE_DELAY));
    
    // Check if measurement is complete (Bit[7] should be 0)
    uint8_t status = 0;
    err = aht20_read_status(dev, &status);
    if (err != ESP_OK) {
        return err;
    }
    
    if (status & AHT20_STATUS_BUSY) {
        ESP_LOGW(TAG, "Sensor still busy");
        return ESP_ERR_TIMEOUT;
    }
    
    // Read 6 bytes of data (7 bytes total with status)
    uint8_t data[7] = {0};
    err = i2c_master_receive(dev->handle, data, sizeof(data), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Read failed");
        return err;
    }
    
    // Extract 20-bit humidity: [1][2][3:4]
    dev->raw_data.humidity = ((uint32_t)data[1] << 12) | 
                             ((uint32_t)data[2] << 4) | 
                             ((uint32_t)data[3] >> 4);
    
    // Extract 20-bit temperature: [3:0-3][4][5]
    dev->raw_data.temperature = (((uint32_t)data[3] & 0x0F) << 16) | 
                                ((uint32_t)data[4] << 8) | 
                                ((uint32_t)data[5]);
    
    return ESP_OK;
}


esp_err_t aht20_compensate(aht20_t *dev)
{
    // Calculate humidity: (raw / 1048576) * 100
    // Store as 0.1% RH units (multiply by 1000)
    dev->res_data.humidity = (dev->raw_data.humidity * 1000) / 10485; // 1048576/100
    
    // Calculate temperature: (raw / 1048576) * 200 - 50
    // Store as 0.1°C units (multiply by 10)
    dev->res_data.temperature = ((dev->raw_data.temperature * 2000) / 10485) - 500;
    
    return ESP_OK;
}


esp_err_t aht20_soft_reset(aht20_t *dev)
{
    uint8_t cmd = AHT20_CMD_SOFT_RESET;
    esp_err_t err = i2c_master_transmit(dev->handle, &cmd, 1, 1000);
    
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(AHT20_RESET_DELAY));
    }
    
    return err;
}


esp_err_t aht20_output(aht20_t *dev)
{
    esp_err_t err;
    
    // Read raw data
    err = aht20_read_raw(dev);
    if (err != ESP_OK) {
        return err;
    }
    
    // Calculate compensated values
    aht20_compensate(dev);
    
    // Display results
    printf("Temperature: %.1f °C\n", dev->res_data.temperature / 10.0);
    printf("Humidity: %.1f %%RH\n", dev->res_data.humidity / 10.0);
    
    return ESP_OK;
}
