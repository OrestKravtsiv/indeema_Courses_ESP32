#pragma once

#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "iot_servo.h"
#define SERVO_GPIO 14  // Servo GPIO

void sg92r_init(void);
void sg92r_set_angle(uint16_t angle);

