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

// MQTT publish function
extern esp_err_t mqtt_publish_data(const char *topic, const char *payload, int qos, bool retain);


#define TAG "APP"


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
    ESP_LOGI(TAG, "Single Click: Switch Mode");
    if (current_mode == MODE_OFF) current_mode = MODE_JOYSTICK;
    else if (current_mode == MODE_JOYSTICK) current_mode = MODE_RAINBOW;
    else current_mode = MODE_JOYSTICK;
}

void on_joy_double_click(void) {
    ESP_LOGI(TAG, "Double Click: FLASHLIGHT MODE");
    if (current_mode != MODE_OFF) current_mode = MODE_WHITE;
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
    
    if (led_strip != NULL) {
        led_strip_set_color(led_strip, (uint8_t)r, (uint8_t)g, (uint8_t)b);
        ESP_LOGI(TAG, "MQTT: LED color set to RGB(%d, %d, %d)", r, g, b);
    } else {
        ESP_LOGE(TAG, "MQTT: LED strip not initialized");
    }
}

void mqtt_get_status(void) {
    const char *mode_names[] = {"JOYSTICK", "RAINBOW", "WHITE", "OFF"};
    const char *wifi_mode_names[] = {"OFF", "STA", "AP", "APSTA"};
    
    ESP_LOGI(TAG, "=== MQTT Status Request ===");
    ESP_LOGI(TAG, "Current Mode: %s", mode_names[current_mode]);
    ESP_LOGI(TAG, "WiFi Mode: %s", wifi_mode_names[current_wifi_mode]);
    ESP_LOGI(TAG, "Free Heap: %u bytes", (unsigned int)esp_get_free_heap_size());
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
    sntp_setup();
    print_current_time();

    // Ініціалізація MQTT
    if (mqtt_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
    }
    // Start MQTT telemetry task
    xTaskCreate(task_mqtt_telemetry, "MQTT_Telemetry", 4096, NULL, 3, NULL);
}