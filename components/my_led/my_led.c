#include <stdio.h>
#include "my_led.h"

led_strip_handle_t led_strip = NULL;

static wifi_led_status_t current_status = LED_STATE_WHITE;
static bool blink_toggle = false;

led_strip_handle_t configure_led(void)
{
    /// LED strip common configuration
    led_strip_config_t strip_config = {
        .strip_gpio_num = 48,  // The GPIO that connected to the LED strip's data line
        .max_leds = 1   ,                 // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812, // LED strip model, it determines the bit timing
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color component format is G-R-B
        .flags = {
            .invert_out = false, // don't invert the output signal
        }
    };


    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .mem_block_symbols = LED_STRIP_MEMORY_BLOCK_WORDS, // the memory block size used by the RMT channel
        .flags = {
            .with_dma = LED_STRIP_USE_DMA,     // Using DMA can improve performance when driving more LEDs
        }
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    return led_strip;
}



void led_strip_set_color(led_strip_handle_t strip, uint8_t r, uint8_t g, uint8_t b)
{
    if (!strip) {
        printf("LED strip not initialized\n");
        return;
    }

    // Встановлюємо колір для пікселя 0
    led_strip_set_pixel(strip, 0, r, g, b);
    led_strip_refresh(strip); // Оновлюємо стрічку, щоб зміни набрали чинності
}

void set_led_status(wifi_led_status_t status) {
    current_status = status;
}

static void led_timer_callback(void* arg) {
    blink_toggle = !blink_toggle;
    uint8_t r = 0, g = 0, b = 0;

    switch (current_status) {
        case LED_STATE_WHITE:       r=255; g=255; b=255; break;
        case LED_STATE_YELLOW:      r=255; g=255; b=0;   break;
        case LED_STATE_RED:         r=255; g=0;   b=0;   break;
        case LED_STATE_GREEN_SOLID: r=0;   g=255; b=0;   break;
        case LED_STATE_BLUE_SOLID:  r=0;   g=0;   b=255; break;
        
        case LED_STATE_GREEN_BLINK:
            if (blink_toggle) { r=0; g=255; b=0; } break;
        case LED_STATE_BLUE_BLINK:
            if (blink_toggle) { r=0; g=0; b=255; } break;
    }
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

void init_led_status_timer(void) {
    const esp_timer_create_args_t timer_args = {
        .callback = &led_timer_callback,
        .name = "led_wifi_timer"
    };
    esp_timer_handle_t timer_handle;
    esp_timer_create(&timer_args, &timer_handle);
    esp_timer_start_periodic(timer_handle, 500000); // 500ms
}