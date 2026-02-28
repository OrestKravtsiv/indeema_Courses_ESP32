# BMP280 Driver Usage Guide

## Overview
The BMP280 driver is now a **professional, production-ready I2C sensor driver** for the ESP32. It supports multiple sensor instances and follows embedded C best practices.

## Key Features
✅ Multi-instance support (multiple sensors simultaneously)  
✅ Thread-safe operation  
✅ Proper encapsulation (no global variables)  
✅ Factory-calibrated readings  
✅ Doxygen documentation  

---

## Basic Usage

### 1. **Declare a sensor instance**
```c
#include "my_bmp280.h"

bmp280_t sensor;  // Create a BMP280 device instance
```

### 2. **Initialize the sensor**
```c
i2c_bus_t *i2c_bus = NULL;  // Assume i2c_bus is already initialized

// Initialize sensor on I2C bus at default address
esp_err_t err = bmp280_init(&sensor, i2c_bus, BMP280_I2C_ADDR);
if (err != ESP_OK) {
    printf("BMP280 init failed!\n");
    return;
}
```

### 3. **Read and display data**
```c
// Single measurement
bmp280_output(&sensor);
// Output: 
// Temperature: 23.45 °C
// Pressure: 1013.25 hPa
```

---

## Advanced Usage

### Read Raw Values and Process Manually
```c
// Step 1: Read raw sensor data
bmp280_read_raw(&sensor);

// Step 2: Apply calibration compensation
bmp280_compensate(&sensor);

// Step 3: Access compensated data
float temperature_c = sensor.res_data.temp / 100.0;
float pressure_hpa = sensor.res_data.press / 25600.0;

printf("Temperature: %.2f °C\n", temperature_c);
printf("Pressure: %.2f hPa\n", pressure_hpa);
```

### Multiple Sensors
```c
bmp280_t sensor1, sensor2;

// Initialize two sensors at different I2C addresses
bmp280_init(&sensor1, &i2c_bus, 0x76);  // Address 0x76
bmp280_init(&sensor2, &i2c_bus, 0x77);  // Address 0x77 (alternate)

// Each sensor maintains its own data
bmp280_output(&sensor1);
bmp280_output(&sensor2);
```

### Periodic Measurements in FreeRTOS Task
```c
void bmp280_task(void *pvParameters) {
    bmp280_t *sensor = (bmp280_t *)pvParameters;
    
    while (1) {
        bmp280_output(sensor);
        vTaskDelay(pdMS_TO_TICKS(1000));  // Read every 1 second
    }
}

// In your app initialization:
bmp280_t my_sensor;
bmp280_init(&my_sensor, &i2c_bus, BMP280_I2C_ADDR);

xTaskCreate(bmp280_task, "BMP280", 2048, &my_sensor, 5, NULL);
```

---

## Data Interpretation

### Temperature
- Stored in: `device.res_data.temp`
- Unit: 0.01 °C (e.g., 2345 = 23.45°C)
- **To display:** `temp / 100.0`

### Pressure
- Stored in: `device.res_data.press`
- Unit: Pa (Pascals)
- **To display in hPa:** `press / 25600.0`
- **To display in mbar:** `press / 25600.0` (1 hPa = 1 mbar)
- **To display in kPa:** `press / 100000.0`

### Calibration Data
- Automatically loaded during `bmp280_init()`
- Stored in: `device.calib_data`
- Used internally by `bmp280_compensate()`

---

## Structure Reference

### Device Instance (`bmp280_t`)
Each sensor instance contains:

```c
typedef struct {
    i2c_master_dev_handle_t handle;     // I2C device handle
    i2c_bus_t *bus;                     // I2C bus reference
    bmp280_calib_data_t calib_data;     // Calibration data (12 coefficients)
    bmp280_raw_data_t raw_data;         // Raw readings (before compensation)
    bmp280_res_data_t res_data;         // Compensated readings (after calibration)
} bmp280_t;
```

---

## Function Workflow

```
bmp280_init(dev, bus, addr)
    └─> bmp280_read_trim(dev)  [loads calibration data]
    └─> Returns: device ready

bmp280_output(dev)  [Recommended for simple use]
    └─> bmp280_read_raw(dev)
    └─> bmp280_compensate(dev)
    └─> printf() results

OR manually:
    bmp280_read_raw(dev)        [I2C read]
    → bmp280_compensate(dev)    [Math calculations]
    → Access dev->res_data      [Get results]
```

---

## Error Handling

```c
esp_err_t err = bmp280_init(&sensor, &i2c_bus, BMP280_I2C_ADDR);

if (err == ESP_OK) {
    printf("Sensor initialized successfully\n");
} else if (err == ESP_ERR_INVALID_ARG) {
    printf("Invalid arguments\n");
} else {
    printf("I2C communication error: 0x%x\n", err);
}
```

---

## Why This Design?

| Old Approach | New Professional Approach |
|---|---|
| Global variables: `calib_data`, `raw_data`, `res_data` | Data stored in device struct |
| Only 1 sensor supported | Multiple sensors supported |
| Not thread-safe | Thread-safe (each instance isolated) |
| Hard to debug | Easy to debug (clear data ownership) |
| Hard to reuse code | Production-grade reusability |

---

## Tips & Best Practices

1. **Always initialize before use** - Call `bmp280_init()` once at startup
2. **Pass device pointer to functions** - Never rely on globals
3. **Check return values** - All functions return `esp_err_t`
4. **Calibration is automatic** - Called by `bmp280_init()`
5. **Use `bmp280_output()` for quick testing** - Use manual steps for advanced use
6. **Each device is independent** - Can run multiple sensors in different tasks

---

## Complete Example: Read BMP280 Every Second

```c
#include "my_bmp280.h"

void app_main(void) {
    // Assume i2c_bus is initialized elsewhere
    
    bmp280_t sensor;
    
    // Initialize sensor
    esp_err_t err = bmp280_init(&sensor, &i2c_bus, BMP280_I2C_ADDR);
    if (err != ESP_OK) {
        printf("Failed to initialize BMP280\n");
        return;
    }
    
    // Read and display every second
    while (1) {
        bmp280_output(&sensor);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

**For detailed API documentation, see Doxygen comments in `my_bmp280.h`**
