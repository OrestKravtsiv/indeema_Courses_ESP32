#include <stdio.h>
#include "my_step.h"

const uint8_t step_sequence[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}
};

void stepper_task(void *pvParameters) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_IN1) | (1ULL << PIN_IN2) | (1ULL << PIN_IN3) | (1ULL << PIN_IN4),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    uint8_t step_index = 0;

    while (1) {
        gpio_set_level(PIN_IN1, step_sequence[step_index][0]);
        gpio_set_level(PIN_IN2, step_sequence[step_index][1]);
        gpio_set_level(PIN_IN3, step_sequence[step_index][2]);
        gpio_set_level(PIN_IN4, step_sequence[step_index][3]);

        step_index++;
        if (step_index >= 8) {
            step_index = 0;
        }
        
        printf("test");
        vTaskDelay(pdMS_TO_TICKS(1000));

    }
}
