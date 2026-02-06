#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

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
    static char task_list_buf[1024];
    static char runtime_buf[1024];

    while (1) {
        printf("\n===== SYSTEM STATUS =====\n");

        printf("\n-- Task List --\n");
        printf("Name          State    Prio    Stack    Num\n");
        vTaskList(task_list_buf);
        printf("%s\n", task_list_buf);

        printf("-- Runtime Stats (CPU usage) --\n");
        printf("Name            Time        %%CPU\n");
        vTaskGetRunTimeStats(runtime_buf);
        printf("%s\n", runtime_buf);

        printf("-- Core Info --\n");
        printf("Logger running on core %d\n", xTaskGetCoreID(NULL));

        printf("==========================\n");

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    static uint32_t task1_params[2] = {1000, 300};
    static uint32_t task2_params[2] = {2000, 500};

    xTaskCreatePinnedToCore(cpu_load_task, "CPU_Load_1", 4096, task1_params, 5, NULL, 0);
    xTaskCreatePinnedToCore(cpu_load_task, "CPU_Load_2", 4096, task2_params, 5, NULL, 0);

    xTaskCreatePinnedToCore(task_logger, "Task_Logger", 4096, NULL, 1, NULL, 1);
}
