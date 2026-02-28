#include "i2c_bus.h"

#define BMP280_I2C_ADDR 0x76
#define BMP280_REG_PRESS_MSB 0xF7
#define BMP280_REG_TEMP_MSB 0xFA
#define BMP280_REG_TRIM_START 0x88


typedef struct{
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
} bmp280_calib_data_t;

typedef struct {
    int32_t temp;
    int32_t press;
} bmp280_raw_data_t;

typedef struct {
    int32_t temp;
    int32_t press;
} bmp280_res_data_t;

typedef struct {
    i2c_master_dev_handle_t handle;
    i2c_bus_t *bus;
    bmp280_calib_data_t calib_data;
    bmp280_raw_data_t raw_data;
    bmp280_res_data_t res_data;
} bmp280_t;

esp_err_t bmp280_init(bmp280_t *dev, i2c_bus_t *bus, uint8_t addr);

esp_err_t bmp280_read_raw(bmp280_t *dev);

esp_err_t bmp280_read_trim(bmp280_t *dev);

esp_err_t bmp280_compensate(bmp280_t *dev);


esp_err_t bmp280_output(bmp280_t *dev);