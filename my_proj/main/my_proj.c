#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

void cpu_load_task(void *pvParameters)
{
    uint32_t period_ms = ((uint32_t *)pvParameters)[0];
    uint32_t load_ms   = ((uint32_t *)pvParameters)[1];

    TickType_t last_wake = xTaskGetTickCount();

    int counter = 0;
    while (1) {
        int64_t start = esp_timer_get_time();
        while ((esp_timer_get_time() - start) < load_ms * 1000) {
            counter++; // Simulate CPU load
        }

        printf("Task: period=%lu ms, load=%lu ms, counter=%d\n", period_ms, load_ms, counter);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(period_ms));
    }
}

void app_main(void)
{
    static uint32_t task1_params[2] = {1000, 300}; // 1s period, 300ms load
    static uint32_t task2_params[2] = {2000, 500}; // 2s period, 500ms load

    xTaskCreate(cpu_load_task, "CPU Load 1", 4096, task1_params, 5, NULL);
    xTaskCreate(cpu_load_task, "CPU Load 2", 4096, task2_params, 5, NULL);
}
