#pragma once

#include "spi_bus.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include <stdint.h>
#include "esp_heap_caps.h"

// LSM6DS3 Registers
#define LSM6DS3_WHO_AM_I        0x0F
#define LSM6DS3_CTRL1_XL        0x10
#define LSM6DS3_CTRL3_C         0x12
#define LSM6DS3_STATUS_REG      0x1E
#define LSM6DS3_OUTX_L_XL       0x28
#define LSM6DS3_OUTX_H_XL       0x29
#define LSM6DS3_OUTY_L_XL       0x2A
#define LSM6DS3_OUTY_H_XL       0x2B
#define LSM6DS3_OUTZ_L_XL       0x2C
#define LSM6DS3_OUTZ_H_XL       0x2D

// Accelerometer configuration values
#define LSM6DS3_ACCEL_2G        0x00
#define LSM6DS3_ACCEL_4G        0x08
#define LSM6DS3_ACCEL_8G        0x0C
#define LSM6DS3_ACCEL_16G       0x04

#define LSM6DS3_ACCEL_ODR_13HZ  0x10
#define LSM6DS3_ACCEL_ODR_26HZ  0x20
#define LSM6DS3_ACCEL_ODR_52HZ  0x30
#define LSM6DS3_ACCEL_ODR_104HZ 0x40
#define LSM6DS3_ACCEL_ODR_208HZ 0x50
#define LSM6DS3_ACCEL_ODR_416HZ 0x60

//SPI Read/Write command bits
#define LSM6DS3_SPI_READ        0x80
#define LSM6DS3_SPI_WRITE       0x00

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} lsm6ds3_accel_t;

typedef struct {
    spi_device_handle_t spi_handle;
    spi_bus_t *bus;
    lsm6ds3_accel_t raw_accel;
    uint8_t range; // 2, 4, 8, or 16 G
} lsm6ds3_t;

// Essential functions
esp_err_t lsm6ds3_init(lsm6ds3_t *dev, spi_bus_t *bus, gpio_num_t cs_pin, uint32_t freq_hz);

esp_err_t lsm6ds3_deinit(lsm6ds3_t *dev);

esp_err_t lsm6ds3_read_accel(lsm6ds3_t *dev);

esp_err_t lsm6ds3_set_config(lsm6ds3_t *dev, uint8_t accel_range, uint8_t accel_odr);

void lsm6ds3_get_accel_data(lsm6ds3_t *dev, float *x, float *y, float *z);
