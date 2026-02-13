#include "my_joystick.h"
#include "esp_log.h"
#include "iot_button.h"
#include "button_gpio.h"

#define TAG "JOYSTICK"

// Глобальні змінні
static adc_oneshot_unit_handle_t s_adc;
static adc_channel_t s_chan_x;
static adc_channel_t s_chan_y;

// Колбеки
static joy_event_cb_t user_single_cb = NULL;
static joy_event_cb_t user_double_cb = NULL;
static joy_event_cb_t user_long_cb = NULL;

// --- Внутрішні обробники ---
static void internal_single_click(void *arg, void *usr_data) {
    if (user_single_cb) user_single_cb();
}

static void internal_double_click(void *arg, void *usr_data) {
    if (user_double_cb) user_double_cb();
}

static void internal_long_press(void *arg, void *usr_data) {
    if (user_long_cb) user_long_cb();
}

// --- Налаштування ADC ---
void configure_adc_pin(int gpio, adc_channel_t *out_channel) {
    adc_unit_t unit = ADC_UNIT_1;
    adc_channel_t channel = ADC_CHANNEL_0;
    ESP_ERROR_CHECK(adc_oneshot_io_to_channel(gpio, &unit, &channel));
    
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, channel, &chan_cfg));
    *out_channel = channel;
}

// --- ГОЛОВНА ФУНКЦІЯ ---
void configure_joystick(joystick_callbacks_t callbacks) {
    user_single_cb = callbacks.on_single_click;
    user_double_cb = callbacks.on_double_click;
    user_long_cb   = callbacks.on_long_press;

    // 1. Налаштування ADC
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));
    configure_adc_pin(JOY_X_GPIO, &s_chan_x);
    configure_adc_pin(JOY_Y_GPIO, &s_chan_y);
    
    button_config_t btn_cfg = {
        .long_press_time = 1500,
        .short_press_time = 200,
    };

    // Б. Конфігурація заліза (GPIO)
    button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = JOY_SW_GPIO,
        .active_level = 0, // 0 = GND (натиснуто)
    };

    button_handle_t btn_handle = NULL;
    esp_err_t err = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn_handle);

    if (btn_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create button");
    } else {
        // УВАГА: Тут додано NULL третім аргументом (event_args)
        
        // Реєстрація одинарного кліка
        iot_button_register_cb(btn_handle, BUTTON_SINGLE_CLICK, NULL, internal_single_click, NULL);
        
        // Реєстрація подвійного кліка
        iot_button_register_cb(btn_handle, BUTTON_DOUBLE_CLICK, NULL, internal_double_click, NULL);
        
        // Реєстрація довгого натискання
        iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_START, NULL, internal_long_press, NULL);
        
        ESP_LOGI(TAG, "Joystick Button Initialized");
    }
}

void read_joystick(int *x, int *y) {
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc, s_chan_x, x));
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc, s_chan_y, y));
}