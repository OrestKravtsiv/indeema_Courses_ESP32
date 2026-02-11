#include <stdio.h>
#include "my_led.h"

led_strip_handle_t configure_led(void)
{
/// LED strip common configuration
led_strip_config_t strip_config = {
    .strip_gpio_num = 48,  // The GPIO that connected to the LED strip's data line
    .max_leds = 1,                 // The number of LEDs in the strip,
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


    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    return led_strip;
}


void led_strip_test(void)
{
    // Ініціалізація драйвера для LED стрічки
    led_strip_handle_t strip = configure_led();
    if (!strip) {
        printf("Failed to initialize LED strip\n");
        return;
    }

    // Встановлюємо червоний колір (R=255, G=0, B=0)
    led_strip_set_pixel(strip, 0, 255, 0, 0); // Піксель 0, R=255, G=0, B=0
    led_strip_refresh(strip); // Оновлюємо стрічку, щоб зміни набрали чинності

    // Затримка для демонстрації
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Вимикаємо світло (R=0, G=0, B=0)
    led_strip_set_pixel(strip, 0, 0, 0, 0); // Піксель 0, R=0, G=0, B=0
    led_strip_refresh(strip); // Оновлюємо стрічку

    // Звільняємо ресурси драйвера
    led_strip_del(strip);
}

void led_strip_blink(void)
{
    led_strip_handle_t strip = configure_led();
    if (!strip) {
        printf("Failed to initialize LED strip\n");
        return;
    }

    while (1) {
        // Встановлюємо червоний колір
        led_strip_set_pixel(strip, 0, 255, 0, 0);
        led_strip_refresh(strip);
        vTaskDelay(pdMS_TO_TICKS(500));

        // Встановлюємо зелений колір
        led_strip_set_pixel(strip, 0, 0, 255, 0);
        led_strip_refresh(strip);
        vTaskDelay(pdMS_TO_TICKS(500));

        // Встановлюємо синій колір
        led_strip_set_pixel(strip, 0, 0, 0, 255);
        led_strip_refresh(strip);
        vTaskDelay(pdMS_TO_TICKS(500));

        // Вимикаємо світло
        led_strip_set_pixel(strip, 0, 0, 0, 0);
        led_strip_refresh(strip);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Звільняємо ресурси драйвера (хоча ми ніколи сюди не дійдемо через безкінечний цикл)
    led_strip_del(strip);
}