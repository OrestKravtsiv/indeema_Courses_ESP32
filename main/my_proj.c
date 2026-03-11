#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "math.h"
#include <inttypes.h>

// БІБЛІОТЕКИ
#include "my_led.h"
#include "my_joystick.h"
#include "my_wifi.h" 
#include "my_mqtt.h"
#include "my_ble.h"
#include "my_sg92r.h"
#include "my_step.h"

// I2C та сенсори
#include "i2c_bus.h"
#include "my_bmp280.h"
#include "my_aht20.h"

// SPI та акселерометр
#include "spi_bus.h"
#include "my_lsm6ds3.h"

// UART
#include "driver/uart.h"
#include "cJSON.h"

#define TAG "APP"

// UART1 конфігурація
#define UART_PORT_NUM      UART_NUM_1
#define UART_TX_PIN        17
#define UART_RX_PIN        18
#define UART_BAUD_RATE     115200
#define UART_BUF_SIZE      512

// I2C конфігурація
#define I2C_MASTER_SCL_IO    9
#define I2C_MASTER_SDA_IO    8
#define I2C_MASTER_NUM       I2C_NUM_0

// SPI конфігурація
#define SPI_MOSI_IO          11
#define SPI_MISO_IO          13
#define SPI_SCLK_IO          12
#define SPI_CS_LSM6DS3       10
#define SPI_HOST_NUM         SPI2_HOST

// Глобальні змінні для I2C та Pressure
static i2c_bus_t i2c_bus;
static bmp280_t bmp280_sensor;
static aht20_t aht20_sensor;

// Глобальні змінні для SPI та акселерометра
static spi_bus_t spi_bus;
static lsm6ds3_t lsm6ds3_sensor;

// ============ TELEMETRY DATA STRUCTURE ============
typedef struct {
    // Sensor data
    float temp_bmp;
    float pressure;
    float temp_aht;
    float humidity;
    float accel_x;
    float accel_y;
    float accel_z;
    
    // System state
    uint32_t free_heap;
    uint8_t battery_level;
    
    // Actuator states
    uint16_t servo_angle;
    uint8_t led_r;
    uint8_t led_g;
    uint8_t led_b;
    bool led_on;
} telemetry_data_t;

telemetry_data_t g_telemetry = {0};

// ============ COMMAND HANDLERS ============

// Servo command handler
void cmd_set_servo_angle(uint16_t angle) {
    if (angle > 180) {
        ESP_LOGW(TAG, "Invalid servo angle: %d (max 180)", angle);
        return;
    }
    g_telemetry.servo_angle = angle;
    sg92r_set_angle(angle);
    ESP_LOGI(TAG, "Servo angle set to: %d", angle);
}

// LED color command handler
void cmd_set_led_color(uint8_t r, uint8_t g, uint8_t b) {
    g_telemetry.led_r = r;
    g_telemetry.led_g = g;
    g_telemetry.led_b = b;
    g_telemetry.led_on = (r != 0 || g != 0 || b != 0);
    
    if (led_strip != NULL) {
        led_strip_set_color(led_strip, r, g, b);
        ESP_LOGI(TAG, "LED color set to RGB(%d, %d, %d)", r, g, b);
    }
}

// MQTT publish function
extern esp_err_t mqtt_publish_data(const char *topic, const char *payload, int qos, bool retain);


void cpu_load_task(void *pvParameters)
{
    uint32_t period_ms = ((uint32_t *)pvParameters)[0];
    uint32_t load_ms   = ((uint32_t *)pvParameters)[1]; 
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        int64_t start = esp_timer_get_time();
        while ((esp_timer_get_time() - start) < load_ms * 1000) {
            __asm__ volatile("nop");
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(period_ms));
    }
}

void task_logger(void *pvParameters)
{
    // Буфери для тексту
    static char task_list_buf[1024];
    static char runtime_buf[1024];

    while (1) {
        printf("\n===== SYSTEM STATUS (Every 5s) =====\n");

        printf("\n-- Task List (Name, State, Prio, Stack, Num, Core) --\n");
        vTaskList(task_list_buf);
        printf("%s\n", task_list_buf);

        printf("-- Runtime Stats (CPU usage) --\n");
        printf("Name            Time            %%CPU\n");
        vTaskGetRunTimeStats(runtime_buf);
        printf("%s\n", runtime_buf);

        printf("------------------------------------\n");
        printf("Free Heap: %u bytes\n", (unsigned int)esp_get_free_heap_size());
        printf("====================================\n");

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ============ SENSOR TASK ============
void task_sensors(void *pvParameters)
{
    char payload[512];
    char topic[64];
    char uart_msg[512];
    
    snprintf(topic, sizeof(topic), "%s/sensors", CONFIG_MQTT_TOPIC_PREFIX);
    
    while (1) {

        // Читаємо дані з BMP280 та AHT20
        bmp280_output(&bmp280_sensor);
        aht20_output(&aht20_sensor);
        
        // Update telemetry structure
        // BMP280: res_data stores values as int32_t (temp in 0.01°C, pressure in Pa)
        g_telemetry.temp_bmp = bmp280_sensor.res_data.temp / 100.0f;
        g_telemetry.pressure = bmp280_sensor.res_data.press / 100.0f; // Convert Pa to hPa
        
        // AHT20: res_data stores values as int32_t (temp in 0.1°C, humidity in 0.1% RH)
        g_telemetry.temp_aht = aht20_sensor.res_data.temperature / 10.0f;
        g_telemetry.humidity = aht20_sensor.res_data.humidity / 10.0f;
        
        // Accelerometer data is updated by accel_task every 100ms,
        // so just use the latest values from g_telemetry here.
        
        // Update system telemetry
        g_telemetry.free_heap = esp_get_free_heap_size();
        
        // Публікуємо в MQTT (вся телеметрія + колір LED)
        snprintf(payload, sizeof(payload),
                "{\"temp_bmp\":%.2f,\"pressure\":%.2f,\"temp_aht\":%.2f,\"humidity\":%.2f,"
                "\"accel_x\":%.2f,\"accel_y\":%.2f,\"accel_z\":%.2f,"
                "\"free_heap\":%lu,\"led_r\":%d,\"led_g\":%d,\"led_b\":%d,\"led_on\":%s,"
                "\"servo_angle\":%d}",
                g_telemetry.temp_bmp, g_telemetry.pressure, g_telemetry.temp_aht, g_telemetry.humidity,
                g_telemetry.accel_x, g_telemetry.accel_y, g_telemetry.accel_z,
                (unsigned long)g_telemetry.free_heap,
                g_telemetry.led_r, g_telemetry.led_g, g_telemetry.led_b,
                g_telemetry.led_on ? "true" : "false",
                g_telemetry.servo_angle);
        
        esp_err_t err = mqtt_publish_data(topic, payload, 1, false);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to publish sensor data");
        }
        
        // Виводимо телеметрію в UART (логи + телеметрія)
        snprintf(uart_msg, sizeof(uart_msg),
                "\n===== TELEMETRY (UART) =====\n"
                "Temperature (BMP280): %.2f °C\n"
                "Pressure: %.2f hPa\n"
                "Temperature (AHT20): %.2f °C\n"
                "Humidity: %.2f %%\n"
                "Accel: X=%.2f, Y=%.2f, Z=%.2f m/s²\n"
                "Free Heap: %lu bytes\n"
                "LED RGB: (%d, %d, %d) - %s\n"
                "Servo Angle: %d°\n"
                "============================\n",
                g_telemetry.temp_bmp, g_telemetry.pressure,
                g_telemetry.temp_aht, g_telemetry.humidity,
                g_telemetry.accel_x, g_telemetry.accel_y, g_telemetry.accel_z,
                (unsigned long)g_telemetry.free_heap,
                g_telemetry.led_r, g_telemetry.led_g, g_telemetry.led_b,
                g_telemetry.led_on ? "ON" : "OFF",
                g_telemetry.servo_angle);
        printf("%s", uart_msg);
        
        vTaskDelay(pdMS_TO_TICKS(5000)); // Читаємо кожні 5 секунд
    }
}

void task_sg92r(void *pvParameters)
{
    while (1) {
        for (uint16_t angle = 0; angle <= 170; angle += 90) {
            sg92r_set_angle(angle);
            vTaskDelay(pdMS_TO_TICKS(1000));
            printf("Set SG92R angle to %d degrees\n", angle);
        }
        for (uint16_t angle = 180; angle >= 10; angle -= 90) {
            sg92r_set_angle(angle);
            vTaskDelay(pdMS_TO_TICKS(1000));
            printf("Set SG92R angle to %d degrees\n", angle);
        }
    }
}


static QueueHandle_t joystick_queue = NULL;

typedef struct {
    int x;
    int y;
} joystick_event_t;

typedef enum {
    MODE_JOYSTICK, 
    MODE_RAINBOW,  
    MODE_WHITE,    
    MODE_OFF       
} app_mode_t;

app_mode_t current_mode;

wifi_mode_t current_wifi_mode = WIFI_MODE_AP; // Початковий режим WiFi

void task_mqtt_telemetry(void *pvParameters)
{
    char payload[256];
    char topic[64];
    const char *mode_names[] = {"JOYSTICK", "RAINBOW", "WHITE", "OFF"};
    const char *wifi_mode_names[] = {"OFF", "STA", "AP", "APSTA"};

    // Build telemetry topic from Kconfig prefix
    snprintf(topic, sizeof(topic), "%s/telemetry", CONFIG_MQTT_TOPIC_PREFIX);

    while (1) {
        // Publish telemetry data at configured interval
        vTaskDelay(pdMS_TO_TICKS(CONFIG_MQTT_TELEMETRY_INTERVAL * 1000));
        
        // Get free heap
        uint32_t free_heap = esp_get_free_heap_size();
        
        // Create JSON telemetry payload
        snprintf(payload, sizeof(payload),
                "{\"mode\":\"%s\",\"wifi_mode\":\"%s\",\"heap\":%" PRIu32 "}",
                mode_names[current_mode],
                wifi_mode_names[current_wifi_mode],
                free_heap);
        
        // Publish telemetry
        esp_err_t err = mqtt_publish_data(topic, payload, 1, false);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to publish telemetry");
        }
    }
}

void task_led(void *pvParameters)
{
    led_strip = configure_led();
    joystick_event_t received_event;
    int hue = 0;


}

void on_joy_single_click(void) {
    ESP_LOGI(TAG, "Single Click: Servo -> 0");
    cmd_set_servo_angle(0);
}

void on_joy_double_click(void) {
    ESP_LOGI(TAG, "Double Click: Servo -> 90");
    cmd_set_servo_angle(90);
}

void on_joy_long_press(void) {
    ESP_LOGI(TAG, "Long Press: POWER TOGGLE");
    if (current_mode == MODE_OFF) current_mode = MODE_JOYSTICK;
    else {
        current_mode = MODE_OFF;
        printf("Going to sleep...\n");
    }
}

// ============ MQTT COMMAND HANDLERS ============

void mqtt_set_mode(int mode) {
    if (mode >= MODE_JOYSTICK && mode <= MODE_OFF) {
        current_mode = (app_mode_t)mode;
        ESP_LOGI(TAG, "MQTT: Mode changed to %d", mode);
    } else {
        ESP_LOGW(TAG, "MQTT: Invalid mode value %d", mode);
    }
}

void mqtt_set_led_color(int r, int g, int b) {
    // Validate RGB values
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        ESP_LOGW(TAG, "MQTT: Invalid RGB values - must be 0-255");
        return;
    }
    
    cmd_set_led_color((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

void mqtt_set_servo_angle(int angle) {
    if (angle < 0 || angle > 180) {
        ESP_LOGW(TAG, "MQTT: Invalid servo angle - must be 0-180");
        return;
    }
    
    cmd_set_servo_angle((uint16_t)angle);
}

void mqtt_get_status(void) {
    const char *mode_names[] = {"JOYSTICK", "RAINBOW", "WHITE", "OFF"};
    const char *wifi_mode_names[] = {"OFF", "STA", "AP", "APSTA"};
    
    ESP_LOGI(TAG, "=== MQTT Status Request ===");
    ESP_LOGI(TAG, "Current Mode: %s", mode_names[current_mode]);
    ESP_LOGI(TAG, "WiFi Mode: %s", wifi_mode_names[current_wifi_mode]);
    ESP_LOGI(TAG, "Free Heap: %u bytes", (unsigned int)esp_get_free_heap_size());
    ESP_LOGI(TAG, "LED RGB: (%d, %d, %d)", g_telemetry.led_r, g_telemetry.led_g, g_telemetry.led_b);
    ESP_LOGI(TAG, "Servo Angle: %d°", g_telemetry.servo_angle);
}

// ============================================

void joystick_task(void *pvParameters)
{
    joystick_callbacks_t my_callbacks = {
        .on_single_click = on_joy_single_click,
        .on_double_click = on_joy_double_click,
        .on_long_press   = on_joy_long_press
    };
    configure_joystick(my_callbacks);
    joystick_event_t event;

    int x = 0, y = 0;
    int history_x = 0;
    int history_y = 0;
    int min_x = 4095;
    int max_x = 0;
    int min_y = 4095;
    int max_y = 0;
    for(int i = 0; i < 20; i++){
        read_joystick(&x, &y);
        history_x += x;
        history_y += y;
        if(min_x > x){
            min_x = x;
        } else if (max_x < x) {
            max_x = x;
        };
        if(min_y > y){
            min_y = y;
        } else if (max_y < y) {
            max_y = y;
        };
    }
    history_x /= 20;
    history_y /= 20;
    min_x -= 100;
    max_x += 100;
    min_y -= 100;
    max_y += 100;

    while (1) {
        read_joystick(&x, &y); 
       
        if (x < min_x && current_wifi_mode != WIFI_MODE_AP) {
            ESP_LOGI("JOY", "Switching to AP Mode...");
            current_wifi_mode = WIFI_MODE_AP;
            switch_wifi_mode(current_wifi_mode);
        } 
        else if (x > max_x && current_wifi_mode != WIFI_MODE_STA) {
            ESP_LOGI("JOY", "Switching to STA Mode...");
            current_wifi_mode = WIFI_MODE_STA;
            switch_wifi_mode(current_wifi_mode);
        }
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}

// ============ BLE STATUS TASK ============
// Періодично надсилає стан ESP через BLE (легкі дані кожні 2 секунди)
void task_ble_status(void *pvParameters)
{
    extern void ble_notify_esp_status(void);
    
    while (1) {
        // Оновлюємо поточний стан системи
        g_telemetry.free_heap = esp_get_free_heap_size();
        g_telemetry.battery_level = CONFIG_BLE_BATTERY_LEVEL; // Можна підключити реальний АЦП
        
        // Надсилаємо статус через BLE
        ble_notify_esp_status();
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // Кожні 2 секунди
    }
}

// ============ UART COMMAND TASK (UART1, JSON) ============
static void uart_send_response(const char *json_str) {
    uart_write_bytes(UART_PORT_NUM, json_str, strlen(json_str));
    uart_write_bytes(UART_PORT_NUM, "\n", 1);
}

static void uart_send_status(void) {
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "cmd", "status");
    cJSON_AddNumberToObject(resp, "led_r", g_telemetry.led_r);
    cJSON_AddNumberToObject(resp, "led_g", g_telemetry.led_g);
    cJSON_AddNumberToObject(resp, "led_b", g_telemetry.led_b);
    cJSON_AddBoolToObject(resp, "led_on", g_telemetry.led_on);
    cJSON_AddNumberToObject(resp, "servo_angle", g_telemetry.servo_angle);
    cJSON_AddNumberToObject(resp, "temp_bmp", g_telemetry.temp_bmp);
    cJSON_AddNumberToObject(resp, "pressure", g_telemetry.pressure);
    cJSON_AddNumberToObject(resp, "temp_aht", g_telemetry.temp_aht);
    cJSON_AddNumberToObject(resp, "humidity", g_telemetry.humidity);
    cJSON_AddNumberToObject(resp, "accel_x", g_telemetry.accel_x);
    cJSON_AddNumberToObject(resp, "accel_y", g_telemetry.accel_y);
    cJSON_AddNumberToObject(resp, "accel_z", g_telemetry.accel_z);
    cJSON_AddNumberToObject(resp, "free_heap", g_telemetry.free_heap);
    char *out = cJSON_PrintUnformatted(resp);
    if (out) {
        uart_send_response(out);
        free(out);
    }
    cJSON_Delete(resp);
}

void uart_command_task(void *pvParameters)
{
    // Configure UART1
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART1 console ready on TX=%d RX=%d @ %d baud", UART_TX_PIN, UART_RX_PIN, UART_BAUD_RATE);

    char line[UART_BUF_SIZE];
    int pos = 0;

    while (1) {
        uint8_t byte;
        int len = uart_read_bytes(UART_PORT_NUM, &byte, 1, pdMS_TO_TICKS(100));
        if (len <= 0) continue;

        if (byte == '\n' || byte == '\r') {
            if (pos == 0) continue;
            line[pos] = '\0';
            pos = 0;

            // Parse JSON: same format as MQTT
            ESP_LOGI(TAG, "UART RX: %s", line);
            cJSON *json = cJSON_Parse(line);
            if (json == NULL) {
                uart_send_response("{\"error\":\"invalid JSON\"}");
                continue;
            }

            cJSON *cmd_type = cJSON_GetObjectItem(json, "cmd");
            if (cmd_type && cmd_type->valuestring) {
                ESP_LOGI(TAG, "UART cmd: %s", cmd_type->valuestring);

                if (strcmp(cmd_type->valuestring, "led_color") == 0) {
                    cJSON *r = cJSON_GetObjectItem(json, "r");
                    cJSON *g = cJSON_GetObjectItem(json, "g");
                    cJSON *b = cJSON_GetObjectItem(json, "b");
                    if (r && g && b) {
                        cmd_set_led_color((uint8_t)r->valueint, (uint8_t)g->valueint, (uint8_t)b->valueint);
                        uart_send_response("{\"ok\":\"led_color\"}");
                    } else {
                        uart_send_response("{\"error\":\"missing r/g/b\"}");
                    }
                }
                else if (strcmp(cmd_type->valuestring, "servo_angle") == 0) {
                    cJSON *angle = cJSON_GetObjectItem(json, "angle");
                    if (angle) {
                        cmd_set_servo_angle((uint16_t)angle->valueint);
                        uart_send_response("{\"ok\":\"servo_angle\"}");
                    } else {
                        uart_send_response("{\"error\":\"missing angle\"}");
                    }
                }
                else if (strcmp(cmd_type->valuestring, "mode") == 0) {
                    cJSON *mode = cJSON_GetObjectItem(json, "value");
                    if (mode && mode->valueint >= 0) {
                        mqtt_set_mode(mode->valueint);
                        uart_send_response("{\"ok\":\"mode\"}");
                    } else {
                        uart_send_response("{\"error\":\"missing value\"}");
                    }
                }
                else if (strcmp(cmd_type->valuestring, "status") == 0) {
                    uart_send_status();
                }
                else {
                    uart_send_response("{\"error\":\"unknown cmd\"}");
                }
            } else {
                uart_send_response("{\"error\":\"missing cmd field\"}");
            }
            cJSON_Delete(json);
        } else {
            if (pos < (int)(sizeof(line) - 1)) {
                line[pos++] = (char)byte;
            }
        }
    }
}

void accel_task(void *pvParameters)
{
    while (1) {
        esp_err_t ret = lsm6ds3_read_accel(&lsm6ds3_sensor);
        if (ret == ESP_OK) {
            lsm6ds3_get_accel_data(&lsm6ds3_sensor, &g_telemetry.accel_x, 
                                   &g_telemetry.accel_y, &g_telemetry.accel_z);
            // Only log when there's actual movement or gravity detected
            if (g_telemetry.accel_x != 0.0f || g_telemetry.accel_y != 0.0f || g_telemetry.accel_z != 0.0f) {
                ESP_LOGI(TAG, "Accelerometer: X=%.2f m/s², Y=%.2f m/s², Z=%.2f m/s²", 
                         g_telemetry.accel_x, g_telemetry.accel_y, g_telemetry.accel_z);
            }
        } else {
            ESP_LOGW(TAG, "Failed to read LSM6DS3 data");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Read every 100ms
    }
}


void app_main(void)
{
    // NVS Init
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize LED hardware and start status indicator timer
    led_strip = configure_led();
    init_led_status_timer();

    // Ініціалізація I2C шини та сенсорів
    ret = i2c_bus_init(&i2c_bus, I2C_MASTER_NUM, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    ESP_LOGI(TAG, "i2c_bus_init: %s, handle: %p", esp_err_to_name(ret), i2c_bus.handle);
    ESP_ERROR_CHECK(ret);

    ret = bmp280_init(&bmp280_sensor, &i2c_bus, BMP280_I2C_ADDR);
    ESP_LOGI(TAG, "bmp280_init: %s", esp_err_to_name(ret));
    ESP_ERROR_CHECK(ret);
    

    // [FIX] AHT20 init
    ret = aht20_init(&aht20_sensor, &i2c_bus, AHT20_I2C_ADDR);
    ESP_ERROR_CHECK(ret);
    
    // Ініціалізація SPI шини та LSM6DS3 акселерометра
    ret = spi_bus_init(&spi_bus, SPI_HOST_NUM, SPI_MOSI_IO, SPI_MISO_IO, SPI_SCLK_IO);
    ESP_LOGI(TAG, "spi_bus_init: %s", esp_err_to_name(ret));
    ESP_ERROR_CHECK(ret);
    
    ret = lsm6ds3_init(&lsm6ds3_sensor, &spi_bus, SPI_CS_LSM6DS3, 1000000); // 1 MHz SPI clock
    ESP_LOGI(TAG, "lsm6ds3_init: %s", esp_err_to_name(ret));
    ESP_ERROR_CHECK(ret);
    
    // Налаштування LSM6DS3: ±8G діапазон, 104 Hz частота
    ret = lsm6ds3_set_config(&lsm6ds3_sensor, LSM6DS3_ACCEL_8G, LSM6DS3_ACCEL_ODR_104HZ);
    ESP_LOGI(TAG, "lsm6ds3_set_config: %s", esp_err_to_name(ret));
    ESP_ERROR_CHECK(ret);

    // Ініціалізація Servo
    sg92r_init();
    ESP_LOGI(TAG, "Servo SG92R initialized");
    g_telemetry.servo_angle = 90; // Початкова позиція
    sg92r_set_angle(90);

    // Запуск WiFi
    wifi_init_combined(current_wifi_mode);

    // Запуск задач
    static uint32_t task1_params[2] = {1000, 300};
    static uint32_t task2_params[2] = {2000, 500};

    xTaskCreatePinnedToCore(cpu_load_task, "CPU_Load_1", 4096, task1_params, 5, NULL, 0);
    xTaskCreatePinnedToCore(cpu_load_task, "CPU_Load_2", 4096, task2_params, 5, NULL, 0);

    // xTaskCreatePinnedToCore(task_logger, "Task_Logger", 4096, NULL, 1, NULL, 1);

    joystick_queue = xQueueCreate(1, sizeof(joystick_event_t));
    if (joystick_queue == NULL) return;

    //xTaskCreate(task_led, "LED_Task", 4096, NULL, 1, NULL);
    xTaskCreate(joystick_task, "Joystick_Task", 4096, NULL, 1, NULL);

    // Запуск задачі для читання сенсорів
    xTaskCreate(task_sensors, "Sensors_Task", 4096, NULL, 3, NULL);
    // xTaskCreate(task_sg92r, "SG92R_Task", 4096, NULL, 3, NULL); // Відключено - тепер керуємо через команди
    // xTaskCreate(stepper_task, "Stepper_Task", 4096, NULL, 3, NULL);

    // Start UART command console (UART1 on GPIO 17/18)
    xTaskCreate(uart_command_task, "UART_Console", 4096, NULL, 1, NULL);
    ESP_LOGI(TAG, "UART console task started");

    sntp_setup();
    print_current_time();

    // Ініціалізація MQTT
    if (mqtt_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
    }
    // Start MQTT telemetry task
    xTaskCreate(task_mqtt_telemetry, "MQTT_Telemetry", 4096, NULL, 3, NULL);

    // Initialize BLE (NimBLE peripheral)
    if (ble_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BLE");
    }
    
    xTaskCreate(accel_task, "Accel_Task", 4096, NULL, 3, NULL);

    // Start BLE status notification task
    xTaskCreate(task_ble_status, "BLE_Status", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "BLE status notification task started");

}