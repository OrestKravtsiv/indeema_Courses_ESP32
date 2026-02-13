
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "iot_button.h"
#include "button_gpio.h"

// Визначаємо тип функції для подій (проста функція без аргументів)
typedef void (*joy_event_cb_t)(void);

// Структура для передачі обробників подій при ініціалізації
typedef struct {
    joy_event_cb_t on_single_click;
    joy_event_cb_t on_double_click;
    joy_event_cb_t on_long_press;
} joystick_callbacks_t;

// Визначення GPIO пінів (змініть на свої)
#define JOY_X_GPIO    5
#define JOY_Y_GPIO    6
#define JOY_SW_GPIO   4



// Оголошення функцій
void configure_adc_pin(int gpio, adc_channel_t *out_channel);
void configure_joystick(joystick_callbacks_t callbacks);
void read_joystick(int *x, int *y);
