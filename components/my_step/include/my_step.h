#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN_IN1 GPIO_NUM_7
#define PIN_IN2 GPIO_NUM_15
#define PIN_IN3 GPIO_NUM_16
#define PIN_IN4 GPIO_NUM_17

void stepper_task(void *pvParameters);
