#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "my_led.h"
#include "my_joystick.h"

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

void task_led(void *pvParameters)
{
    led_strip = configure_led();
    while (1) {
        led_strip_set_color(led_strip, 255, 0, 0); // Червоний колір
        vTaskDelay(pdMS_TO_TICKS(1000)); // Затримка між тестами
        led_strip_set_color(led_strip, 0, 255, 0); // Зелений колір
        vTaskDelay(pdMS_TO_TICKS(1000)); // Затримка між тестами
        led_strip_set_color(led_strip, 0, 0, 255); // Синій колір
        vTaskDelay(pdMS_TO_TICKS(1000)); // Затримка між тестами
    }
}


void app_main(void)
{
    static uint32_t task1_params[2] = {1000, 300};
    static uint32_t task2_params[2] = {2000, 500};

    xTaskCreatePinnedToCore(cpu_load_task, "CPU_Load_1", 4096, task1_params, 5, NULL, 0);
    xTaskCreatePinnedToCore(cpu_load_task, "CPU_Load_2", 4096, task2_params, 5, NULL, 0);

    xTaskCreatePinnedToCore(task_logger, "Task_Logger", 4096, NULL, 1, NULL, 1);

    xTaskCreate(task_led, "LED_Test", 4096, NULL, 1, NULL);
}
