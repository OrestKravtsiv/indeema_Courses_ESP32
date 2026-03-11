#include <stdio.h>
#include "my_sg92r.h"

static uint16_t calibration_value_0 = 30;
static uint16_t calibration_value_180 = 195;

static servo_config_t servo_cfg = {
    .max_angle = 180,
    .min_width_us = 500,
    .max_width_us = 2500,
    .freq = 50,
    .timer_number = LEDC_TIMER_0,
    .channels = {
        .servo_pin = {
            SERVO_GPIO,
        },
        .ch = {
            LEDC_CHANNEL_0,
        },
    },
    .channel_number = 1,
};

static inline uint16_t map(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void sg92r_init(void) {
    ESP_LOGI("SG92R", "Initializing SG92R Servo...");
    iot_servo_init(LEDC_LOW_SPEED_MODE, &servo_cfg);
}

void sg92r_set_angle(uint16_t angle) {
    if (angle > servo_cfg.max_angle) {
        ESP_LOGW("SG92R", "Angle %d exceeds max angle %d, setting to max", angle, servo_cfg.max_angle);
        angle = servo_cfg.max_angle;
    }
    uint16_t calibrated_angle = map(angle, 0, servo_cfg.max_angle, calibration_value_0, calibration_value_180);
    iot_servo_write_angle(LEDC_LOW_SPEED_MODE, 0, calibrated_angle);
}

