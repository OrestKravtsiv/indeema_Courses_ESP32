
# LSM6DS3 SPI Accelerometer Driver

A minimal ESP-IDF driver for the STMicroelectronics LSM6DS3 6-axis IMU (Inertial Measurement Unit) communicating via SPI.

## Overview

The LSM6DS3 is a 6-axis IMU containing a 3-axis accelerometer and 3-axis gyroscope. This component provides essential functions to initialize, configure, and read acceleration data over SPI.

## Features

- **SPI Communication**: Full duplex SPI interface support
- **Accelerometer Operations**: Initialize, configure range/ODR, and read raw acceleration data
- **Flexible Range**: Support for ±2g, ±4g, ±8g, and ±16g full-scale ranges
- **Configurable ODR**: Output data rates from 13 Hz to 416 Hz
- **Data Conversion**: Raw register values to physical units (m/s²)

## Hardware Connections

Connect the LSM6DS3 to your ESP32-S3 board:

| LSM6DS3 Pin | ESP32-S3 Pin | Description |
|-------------|-------------|-------------|
| VCC         | 3.3V        | Power supply |
| GND         | GND         | Ground |
| SCL/SPC     | GPIO17      | SPI Clock |
| SDA/MOSI    | GPIO11      | SPI MOSI |
| SDO/MISO    | GPIO13      | SPI MISO |
| CS          | GPIO12      | Chip Select |

*Note: Pin numbers are configurable - adjust in your application code.*

## API Reference

### Initialization

#### `lsm6ds3_init()`
```c
esp_err_t lsm6ds3_init(lsm6ds3_t *dev, spi_bus_t *bus, gpio_num_t cs_pin, uint32_t freq_hz)
```
Initialize the LSM6DS3 sensor.

**Parameters:**
- `dev`: Pointer to device structure
- `bus`: Pointer to SPI bus instance
- `cs_pin`: Chip select GPIO pin
- `freq_hz`: SPI clock frequency (typical: 1-10 MHz)

**Returns:** `ESP_OK` on success, error code otherwise

---

#### `lsm6ds3_deinit()`
```c
esp_err_t lsm6ds3_deinit(lsm6ds3_t *dev)
```
Deinitialize and free SPI device resources.

**Returns:** `ESP_OK` on success

---

### Configuration

#### `lsm6ds3_set_config()`
```c
esp_err_t lsm6ds3_set_config(lsm6ds3_t *dev, uint8_t accel_range, uint8_t accel_odr)
```
Configure accelerometer range and output data rate.

**Parameters:**
- `accel_range`: Full-scale range (`LSM6DS3_ACCEL_2G`, `LSM6DS3_ACCEL_4G`, `LSM6DS3_ACCEL_8G`, `LSM6DS3_ACCEL_16G`)
- `accel_odr`: Output data rate (`LSM6DS3_ACCEL_ODR_13HZ` to `LSM6DS3_ACCEL_ODR_416HZ`)

**Example:**
```c
lsm6ds3_set_config(&dev, LSM6DS3_ACCEL_8G, LSM6DS3_ACCEL_104HZ);
```

---

### Data Reading

#### `lsm6ds3_read_accel()`
```c
esp_err_t lsm6ds3_read_accel(lsm6ds3_t *dev)
```
Read raw acceleration values from the sensor (stores in `dev->raw_accel`).

**Returns:** `ESP_OK` on success

---

#### `lsm6ds3_get_accel_data()`
```c
void lsm6ds3_get_accel_data(lsm6ds3_t *dev, float *x, float *y, float *z)
```
Get acceleration values in m/s² (converted from raw register values).

**Parameters:**
- `x`, `y`, `z`: Pointers to store acceleration values

---

## Data Types

### `lsm6ds3_accel_t`
```c
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} lsm6ds3_accel_t;
```

### `lsm6ds3_t`
```c
typedef struct {
    spi_device_handle_t spi_handle;
    spi_bus_t *bus;
    lsm6ds3_accel_t raw_accel;
    uint8_t range; // 2, 4, 8, or 16 G
} lsm6ds3_t;
```

## Usage Example

```c
#include "my_lsm6ds3.h"

void app_main(void)
{
    // Initialize SPI bus
    spi_bus_t spi_bus = {
        .mosi_io_num = GPIO_NUM_11,
        .miso_io_num = GPIO_NUM_13,
        .sclk_io_num = GPIO_NUM_17,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1024,
    };
    spi_bus_init(&spi_bus, SPI2_HOST);

    // Initialize LSM6DS3
    lsm6ds3_t lsm6ds3_dev;
    ESP_ERROR_CHECK(lsm6ds3_init(&lsm6ds3_dev, &spi_bus, GPIO_NUM_12, 1000000));

    // Configure: 8G range, 104 Hz ODR
    ESP_ERROR_CHECK(lsm6ds3_set_config(&lsm6ds3_dev, LSM6DS3_ACCEL_8G, LSM6DS3_ACCEL_104HZ));

    // Read data in a loop
    for (int i = 0; i < 100; i++) {
        ESP_ERROR_CHECK(lsm6ds3_read_accel(&lsm6ds3_dev));

        float x, y, z;
        lsm6ds3_get_accel_data(&lsm6ds3_dev, &x, &y, &z);
        printf("X: %.2f m/s², Y: %.2f m/s², Z: %.2f m/s²\n", x, y, z);

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Cleanup
    lsm6ds3_deinit(&lsm6ds3_dev);
}
```

## Component Configuration

Add to your `CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "my_lsm6ds3.c"
    INCLUDE_DIRS "include"
    REQUIRES driver spi_bus esp_common
)
```

## Register Definitions

Key registers used:
- `0x0F`: WHO_AM_I - Device identification
- `0x10`: CTRL1_XL - Accelerometer control
- `0x12`: CTRL3_C - Device control
- `0x28-0x2D`: OUTX/Y/Z_L/H_XL - Acceleration output registers

## Notes

- The LSM6DS3 requires a SPI bus initialized with `spi_bus_init()`
- CS pin is automatically managed by the SPI driver
- Acceleration values are in m/s² following standard SI units
- The sensor supports both SPI and I2C - this driver uses SPI only
- Typical I2C address: 0x6A or 0x6B (SPI mode doesn't use addressing)

## References

- [LSM6DS3 Datasheet](https://www.st.com/resource/en/datasheet/lsm6ds3.pdf)
- [ESP-IDF SPI Master Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/spi_master.html)
