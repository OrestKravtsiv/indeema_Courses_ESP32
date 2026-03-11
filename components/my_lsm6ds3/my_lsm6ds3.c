#include "my_lsm6ds3.h"
#include <string.h>

static const char *TAG = "LSM6DS3";
#define LSM6DS3_CTRL3_BDU    0x40
#define LSM6DS3_CTRL3_IF_INC 0x04

static esp_err_t lsm6ds3_add_device(lsm6ds3_t *dev, spi_bus_t *bus, gpio_num_t cs_pin, uint32_t freq_hz, uint8_t mode)
{
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = freq_hz,           // Use provided frequency
        .mode = mode,                        // SPI mode from parameter
        .spics_io_num = cs_pin,              // CS pin
        .queue_size = 7,
    };
    esp_err_t err = spi_bus_add_device(bus->host, &devcfg, &dev->spi_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device");
        return err;
    }
    return ESP_OK;
}

// Допоміжна функція для запису одного регістра
static esp_err_t lsm6ds3_write_reg(lsm6ds3_t *dev, uint8_t reg, uint8_t value)
{
    if (!dev || !dev->spi_handle) return ESP_ERR_INVALID_STATE;

    uint8_t tx_data[2] = {(reg & 0x7F) | LSM6DS3_SPI_WRITE, value};
    
    spi_transaction_t txn = {
        .length = 16,
        .tx_buffer = tx_data,
        .rx_buffer = NULL,
    };
    
    return spi_device_polling_transmit(dev->spi_handle, &txn);
}

// Допоміжна функція для читання одного регістра
static esp_err_t lsm6ds3_read_reg(lsm6ds3_t *dev, uint8_t reg, uint8_t *value)
{
    if (!dev || !dev->spi_handle || !value) return ESP_ERR_INVALID_STATE;

    uint8_t tx_data[2] = {(reg & 0x7F) | LSM6DS3_SPI_READ, 0};
    uint8_t rx_data[2] = {0};
    
    spi_transaction_t txn = {
        .length = 16,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    
    esp_err_t ret = spi_device_polling_transmit(dev->spi_handle, &txn);
    if (ret == ESP_OK) {
        *value = rx_data[1];
    }
    return ret;
}

// Допоміжна функція для читання кількох регістрів
static esp_err_t lsm6ds3_read_regs(lsm6ds3_t *dev, uint8_t reg, uint8_t *data, uint8_t len)
{
    if (!dev || !dev->spi_handle || !data || len == 0) return ESP_ERR_INVALID_STATE;

    // Max burst in this driver is 6 bytes + 1 address byte.
    if (len > 6) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx_data[7] = {0};
    uint8_t rx_data[7] = {0};
    
    // Auto-increment is enabled via IF_INC bit in CTRL3_C register.
    // SPI command: bit7=R/W, bits6-0=address. No auto-inc bit in command byte.
    tx_data[0] = (reg & 0x7F) | LSM6DS3_SPI_READ;
    
    spi_transaction_t txn = {
        .length = (len + 1) * 8,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    
    esp_err_t ret = spi_device_polling_transmit(dev->spi_handle, &txn);
    if (ret == ESP_OK) {
        memcpy(data, &rx_data[1], len);
    }
    return ret;
}

esp_err_t lsm6ds3_init(lsm6ds3_t *dev, spi_bus_t *bus, gpio_num_t cs_pin, uint32_t freq_hz)
{
    if (!dev || !bus) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    dev->bus = bus;
    dev->range = 2; // Default to 2G
    
    esp_err_t ret = ESP_FAIL;
    uint8_t who_am_i = 0;
    uint8_t detected_mode = 0;

    for (int attempt = 0; attempt < 2; attempt++) {
        uint8_t spi_mode = (attempt == 0) ? 0 : 3;

        ret = lsm6ds3_add_device(dev, bus, cs_pin, freq_hz, spi_mode);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add SPI device (mode %d)", spi_mode);
            if(attempt == 0) {
                continue;
            }
            return ret;
        }

        vTaskDelay(pdMS_TO_TICKS(10));

        uint8_t test_regs[5] = {0};
        ESP_LOGI(TAG, "=== SPI Communication Test (mode %d) ===", spi_mode);
        for (int i = 0; i < 5; i++) {
            ret = lsm6ds3_read_reg(dev, (uint8_t)(0x0F + i), &test_regs[i]);
            if (ret != ESP_OK) {
                break;
            }
            ESP_LOGI(TAG, "Reg 0x%02X = 0x%02X", 0x0F + i, test_regs[i]);
        }

        if (ret == ESP_OK) {
            who_am_i = test_regs[0];
            ESP_LOGI(TAG, "WHO_AM_I = 0x%02X", who_am_i);
            if (who_am_i == 0x69 || who_am_i == 0x6A) {
                detected_mode = spi_mode;
                break;
            }
        }

        spi_bus_remove_device(dev->spi_handle);
        dev->spi_handle = NULL;
    }
    
    if (who_am_i != 0x69 && who_am_i != 0x6A) {
        ESP_LOGE(TAG, "Invalid WHO_AM_I: 0x%02X (expected 0x69 for LSM6DS3 or 0x6A for LSM6DSL)", who_am_i);
        ESP_LOGI(TAG, "Note: 0x6A=LSM6DSL, 0x6C=LSM6DSO, 0x69=LSM6DS3");
        if (dev->spi_handle) {
            spi_bus_remove_device(dev->spi_handle);
            dev->spi_handle = NULL;
        }
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "LSM6DS3 found (WHO_AM_I: 0x%02X, SPI mode: %d)", who_am_i, detected_mode);
    
    // Set default configuration: 2G range, 104 Hz ODR
    ret = lsm6ds3_set_config(dev, LSM6DS3_ACCEL_2G, LSM6DS3_ACCEL_ODR_104HZ);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set configuration");
        spi_bus_remove_device(dev->spi_handle);
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t lsm6ds3_deinit(lsm6ds3_t *dev)
{
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (dev->spi_handle) {
        esp_err_t ret = spi_bus_remove_device(dev->spi_handle);
        dev->spi_handle = NULL;
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t lsm6ds3_read_accel(lsm6ds3_t *dev)
{
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if new accelerometer data is available
    uint8_t status = 0;
    esp_err_t ret = lsm6ds3_read_reg(dev, LSM6DS3_STATUS_REG, &status);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read STATUS_REG");
        return ret;
    }
    
    // Bit 0 = XLDA (Accelerometer data available)
    if (!(status & 0x01)) {
        // No new data available - return last reading without error
        return ESP_OK;
    }
    
    uint8_t data[6];
    
    // Read all 6 accelerometer registers in one burst
    ret = lsm6ds3_read_regs(dev, LSM6DS3_OUTX_L_XL, data, 6);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read accelerometer data");
        return ret;
    }
    
    // Combine low and high bytes
    dev->raw_accel.x = (int16_t)((data[1] << 8) | data[0]);
    dev->raw_accel.y = (int16_t)((data[3] << 8) | data[2]);
    dev->raw_accel.z = (int16_t)((data[5] << 8) | data[4]);
    
    // Debug: log raw values when non-zero
    if (dev->raw_accel.x != 0 || dev->raw_accel.y != 0 || dev->raw_accel.z != 0) {
        // ESP_LOGI(TAG, "Raw data: [%02X %02X %02X %02X %02X %02X] -> X=%d Y=%d Z=%d",
                //  data[0], data[1], data[2], data[3], data[4], data[5],
                //  dev->raw_accel.x, dev->raw_accel.y, dev->raw_accel.z);
    }
    
    return ESP_OK;
}

esp_err_t lsm6ds3_set_config(lsm6ds3_t *dev, uint8_t accel_range, uint8_t accel_odr)
{
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Write CTRL1_XL register: set ODR and range
    // Bits 6-4: ODR, Bits 3-2: Range
    uint8_t ctrl1_xl = accel_odr | accel_range;
    
    esp_err_t ret = lsm6ds3_write_reg(dev, LSM6DS3_CTRL1_XL, ctrl1_xl);
    vTaskDelay(pdMS_TO_TICKS(10));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure CTRL1_XL");
        return ret;
    }

    // Enable block-data-update + auto-increment for stable multi-byte reads.
    ret = lsm6ds3_write_reg(dev, LSM6DS3_CTRL3_C, LSM6DS3_CTRL3_BDU | LSM6DS3_CTRL3_IF_INC);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure CTRL3_C");
        return ret;
    }
    
    // Update device range
    switch (accel_range) {
        case LSM6DS3_ACCEL_2G:
            dev->range = 2;
            break;
        case LSM6DS3_ACCEL_4G:
            dev->range = 4;
            break;
        case LSM6DS3_ACCEL_8G:
            dev->range = 8;
            break;
        case LSM6DS3_ACCEL_16G:
            dev->range = 16;
            break;
        default:
            dev->range = 2;
    }
    
    ESP_LOGI(TAG, "Configured: Range=%dG, ODR=0x%02X", dev->range, accel_odr);
    
    return ESP_OK;
}

void lsm6ds3_get_accel_data(lsm6ds3_t *dev, float *x, float *y, float *z)
{
    if (!dev || !x || !y || !z) {
        return;
    }
    
    // LSM6DS3 sensitivity in mg/LSB depends on full-scale range
    // ±2G: 0.061 mg/LSB, ±4G: 0.122 mg/LSB, ±8G: 0.244 mg/LSB, ±16G: 0.488 mg/LSB
    float sensitivity_mg;
    switch (dev->range) {
        case 2:  sensitivity_mg = 0.061f; break;
        case 4:  sensitivity_mg = 0.122f; break;
        case 8:  sensitivity_mg = 0.244f; break;
        case 16: sensitivity_mg = 0.488f; break;
        default: sensitivity_mg = 0.061f; break;
    }
    
    // Convert to m/s² (1g = 9.81 m/s²)
    float sensitivity = (sensitivity_mg / 1000.0f) * 9.81f;
    
    *x = dev->raw_accel.x * sensitivity;
    *y = dev->raw_accel.y * sensitivity;
    *z = dev->raw_accel.z * sensitivity;

}
