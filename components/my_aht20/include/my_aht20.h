#include "i2c_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// AHT20 I2C Address and Commands
#define AHT20_I2C_ADDR       0x38    /**< I2C slave address */
#define AHT20_CMD_INIT       0xBE    /**< Initialization command */
#define AHT20_CMD_TRIGGER    0xAC    /**< Trigger measurement command */
#define AHT20_CMD_SOFT_RESET 0xBA    /**< Soft reset command */
#define AHT20_CMD_STATUS     0x71    /**< Read status command */

// AHT20 Status Bits
#define AHT20_STATUS_BUSY    0x80    /**< Bit[7]: 1=busy, 0=idle */
#define AHT20_STATUS_CAL     0x08    /**< Bit[3]: 1=calibrated */

// Timing delays (milliseconds)
#define AHT20_POWER_ON_DELAY 40      /**< Wait after power-on */
#define AHT20_INIT_DELAY     10      /**< Wait after init command */
#define AHT20_MEASURE_DELAY  80      /**< Wait for measurement */
#define AHT20_RESET_DELAY    20      /**< Wait after soft reset */


typedef struct {
    uint32_t humidity;    /**< Raw humidity data (20-bit) */
    uint32_t temperature; /**< Raw temperature data (20-bit) */
} aht20_raw_data_t;

typedef struct {
    int32_t humidity;    /**< Humidity in 0.1% RH */
    int32_t temperature; /**< Temperature in 0.1°C */
} aht20_res_data_t;

typedef struct {
    i2c_master_dev_handle_t handle;  /**< I2C device handle */
    i2c_bus_t *bus;                  /**< I2C bus reference */
    aht20_raw_data_t raw_data;       /**< Raw readings */
    aht20_res_data_t res_data;       /**< Compensated readings */
} aht20_t;

esp_err_t aht20_init(aht20_t *dev, i2c_bus_t *bus, uint8_t addr);


esp_err_t aht20_read_status(aht20_t *dev, uint8_t *status);

esp_err_t aht20_is_calibrated(aht20_t *dev, bool *calibrated);

esp_err_t aht20_trigger_measurement(aht20_t *dev);

esp_err_t aht20_read_raw(aht20_t *dev);

esp_err_t aht20_compensate(aht20_t *dev);

esp_err_t aht20_soft_reset(aht20_t *dev);

esp_err_t aht20_output(aht20_t *dev);