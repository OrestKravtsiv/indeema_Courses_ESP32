#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h" // Додано для EventGroup
#include "esp_log.h"
#include "esp_timer.h"
#include "math.h"

// БІБЛІОТЕКИ
#include "my_led.h"
#include "my_joystick.h"
#include "my_wifi.h" 


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

        // Оскільки ви увімкнули показ ядра в меню-конфігураторі, 
        // vTaskList сама додасть колонку з номером ядра або символом 'X' (якщо без прив'язки)
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

app_mode_t current_mode = MODE_JOYSTICK;

void task_led(void *pvParameters)
{
    led_strip = configure_led();
    joystick_event_t received_event;
    int hue = 0;

    while (1) {
        if (xQueueReceive(joystick_queue, &received_event, portMAX_DELAY) && current_mode == MODE_JOYSTICK) {
            int raw_x = received_event.x;
            int raw_y = received_event.y;
            uint8_t r = 0, g = 0, b = 0;

            if (raw_x < 2048) {
                int val = (raw_x * 255) / 2048; 
                g = 255 - val; b = val; r = 0;
            } else {
                int val = ((raw_x - 2048) * 255) / 2048;
                b = 255 - val; r = val; g = 0;
            }

            float brightness = (float)raw_y / 4095.0f;
            r = (uint8_t)(r * brightness);
            g = (uint8_t)(g * brightness);
            b = (uint8_t)(b * brightness);

            led_strip_set_pixel(led_strip, 0, r, g, b);
            led_strip_refresh(led_strip);
        }
        else if (current_mode == MODE_RAINBOW) {
            hue += 5; 
            if (hue > 360) hue = 0;
            float rad = hue * (M_PI / 180.0);
            uint8_t r = (uint8_t)(sin(rad) * 127 + 128);
            uint8_t g = (uint8_t)(sin(rad + 2*M_PI/3) * 127 + 128);
            uint8_t b = (uint8_t)(sin(rad + 4*M_PI/3) * 127 + 128);
            led_strip_set_pixel(led_strip, 0, r, g, b);
            led_strip_refresh(led_strip);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        else if (current_mode == MODE_WHITE) {
            led_strip_set_pixel(led_strip, 0, 255, 255, 255);
            led_strip_refresh(led_strip);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        else if (current_mode == MODE_OFF) {
            led_strip_set_pixel(led_strip, 0, 0, 0, 0);
            led_strip_refresh(led_strip);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
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

void joystick_task(void *pvParameters)
{
    joystick_callbacks_t my_callbacks = {
        .on_single_click = on_joy_single_click,
        .on_double_click = on_joy_double_click,
        .on_long_press   = on_joy_long_press
    };
    configure_joystick(my_callbacks);
    joystick_event_t event;

    while (1) {
       int x, y;
       read_joystick(&x, &y); 
       event.x = x; event.y = y;
       xQueueOverwrite(joystick_queue, &event);
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

    // Запуск WiFi (все в одній коректній функції)
    wifi_init_combined();

    // Запуск задач
    static uint32_t task1_params[2] = {1000, 300};
    static uint32_t task2_params[2] = {2000, 500};

    xTaskCreatePinnedToCore(cpu_load_task, "CPU_Load_1", 4096, task1_params, 5, NULL, 0);
    xTaskCreatePinnedToCore(cpu_load_task, "CPU_Load_2", 4096, task2_params, 5, NULL, 0);

    // xTaskCreatePinnedToCore(task_logger, "Task_Logger", 4096, NULL, 1, NULL, 1);

    joystick_queue = xQueueCreate(1, sizeof(joystick_event_t));
    if (joystick_queue == NULL) return;

    xTaskCreate(task_led, "LED_Task", 4096, NULL, 1, NULL);
    xTaskCreate(joystick_task, "Joystick_Task", 4096, NULL, 1, NULL);
}