# AHT20 Driver Usage Guide

## Overview
The AHT20 driver is a **professional, production-ready I2C sensor driver** for the ESP32. It supports multiple sensor instances and follows embedded C best practices for temperature and humidity measurements.

## Key Features
✅ Multi-instance support (multiple sensors simultaneously)  
✅ Thread-safe operation  
✅ Proper encapsulation (no global variables)  
✅ Factory-calibrated readings  
✅ Doxygen documentation  
✅ Temperature and humidity compensation  

---

## Basic Usage

### 1. **Declare a sensor instance**
```c
#include "my_aht20.h"

aht20_t sensor;  // Create an AHT20 device instance
```

### 2. **Initialize the sensor**
```c
i2c_bus_t *i2c_bus = NULL;  // Assume i2c_bus is already initialized

// Initialize sensor on I2C bus at default address (0x38)
esp_err_t err = aht20_init(&sensor, i2c_bus, AHT20_I2C_ADDR);
if (err != ESP_OK) {
    printf("AHT20 init failed!\n");
    return;
}
```

### 3. **Read and display data**
```c
// Single measurement
aht20_output(&sensor);
// Output: 
// Temperature: 23.45 °C
// Humidity: 65.30 %RH
```

---

## Advanced Usage

### Read Raw Values and Process Manually
```c
// Step 1: Read raw sensor data
aht20_read_raw(&sensor);

// Step 2: Apply calibration compensation
aht20_compensate(&sensor);

// Step 3: Access compensated data
float temperature_c = sensor.res_data.temp / 100.0;
float humidity_rh = sensor.res_data.humidity / 100.0;

printf("Temperature: %.2f °C\n", temperature_c);
printf("Humidity: %.2f %%RH\n", humidity_rh);
```

### Multiple Sensors
```c
aht20_t sensor1, sensor2;

// Initialize two sensors at the same address on different I2C buses
// (Note: AHT20 has fixed address 0x38, so multiple sensors need separate buses)
aht20_init(&sensor1, &i2c_bus1, AHT20_I2C_ADDR);
aht20_init(&sensor2, &i2c_bus2, AHT20_I2C_ADDR);

// Each sensor maintains its own data
aht20_output(&sensor1);
aht20_output(&sensor2);
```

### Periodic Measurements in FreeRTOS Task
```c
void aht20_task(void *pvParameters) {
    aht20_t *sensor = (aht20_t *)pvParameters;
    
    while (1) {
        aht20_output(sensor);
        vTaskDelay(pdMS_TO_TICKS(1000));  // Read every 1 second
    }
}

// In your app initialization:
aht20_t my_sensor;
aht20_init(&my_sensor, &i2c_bus, AHT20_I2C_ADDR);

xTaskCreate(aht20_task, "AHT20", 2048, &my_sensor, 5, NULL);
```

---

## Data Interpretation

### Temperature
- Stored in: `device.res_data.temp`
- Unit: 0.01 °C (e.g., 2345 = 23.45°C)
- **To display:** `temp / 100.0`
- **Range:** -40 to +85°C

### Humidity
- Stored in: `device.res_data.humidity`
- Unit: 0.01 %RH (e.g., 6530 = 65.30%RH)
- **To display:** `humidity / 100.0`
- **Range:** 0 to 100%RH

### Calibration Data
- Automatically loaded during `aht20_init()`
- Stored in: `device.calib_data`
- Used internally by `aht20_compensate()`

---

## Structure Reference

### Device Instance (`aht20_t`)
Each sensor instance contains:

```c
typedef struct {
    i2c_master_dev_handle_t handle;     // I2C device handle
    i2c_bus_t *bus;                     // I2C bus reference
    aht20_calib_data_t calib_data;      // Calibration data
    aht20_raw_data_t raw_data;          // Raw readings (before compensation)
    aht20_res_data_t res_data;          // Compensated readings (after calibration)
} aht20_t;
```

---

## Function Workflow

```
aht20_init(dev, bus, addr)
    └─> aht20_read_calib_data(dev)  [loads calibration data]
    └─> Returns: device ready

aht20_output(dev)  [Recommended for simple use]
    └─> aht20_read_raw(dev)
    └─> aht20_compensate(dev)
    └─> printf() results

OR manually:
    aht20_read_raw(dev)        [I2C read + status check]
    → aht20_compensate(dev)    [Math calculations]
    → Access dev->res_data     [Get results]
```

---

## Error Handling

```c
esp_err_t err = aht20_init(&sensor, &i2c_bus, AHT20_I2C_ADDR);

if (err == ESP_OK) {
    printf("Sensor initialized successfully\n");
} else if (err == ESP_ERR_INVALID_ARG) {
    printf("Invalid arguments\n");
} else if (err == ESP_ERR_TIMEOUT) {
    printf("I2C communication timeout\n");
} else {
    printf("I2C communication error: 0x%x\n", err);
}
```

---

## Why This Design?

| Old Approach | New Professional Approach |
|---|---|
| Global variables: `temp`, `humidity` | Data stored in device struct |
| Only 1 sensor supported | Multiple sensors supported |
| Not thread-safe | Thread-safe (each instance isolated) |
| Hard to debug | Easy to debug (clear data ownership) |
| Hard to reuse code | Production-grade reusability |

---

## Tips & Best Practices

1. **Always initialize before use** - Call `aht20_init()` once at startup
2. **Pass device pointer to functions** - Never rely on globals
3. **Check return values** - All functions return `esp_err_t`
4. **Calibration is automatic** - Called by `aht20_init()`
5. **Use `aht20_output()` for quick testing** - Use manual steps for advanced use
6. **Each device is independent** - Can run multiple sensors in different tasks
7. **Measurement time** - Allow ~80ms between measurements for accurate readings
8. **Temperature affects humidity** - Both values are interdependent; read both

---

## Complete Example: Read AHT20 Every Second

```c
#include "my_aht20.h"

void app_main(void) {
    // Assume i2c_bus is initialized elsewhere
    
    aht20_t sensor;
    
    // Initialize sensor
    esp_err_t err = aht20_init(&sensor, &i2c_bus, AHT20_I2C_ADDR);
    if (err != ESP_OK) {
        printf("Failed to initialize AHT20\n");
        return;
    }
    
    // Read and display every second
    while (1) {
        aht20_output(&sensor);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---
