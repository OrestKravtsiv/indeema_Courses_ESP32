#include "led_strip.h"
#include "esp_timer.h"
#define LED_STRIP_USE_DMA  0

#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)
#define LED_STRIP_MEMORY_BLOCK_WORDS 0 // let the driver choose a proper memory block size automatically

extern led_strip_handle_t led_strip;

// --- СТАНИ ДЛЯ ІНДИКАТОРА WIFI ---
typedef enum {
    LED_STATE_WHITE,        // Wi-Fi OFF
    LED_STATE_YELLOW,       // Підключення (STA)
    LED_STATE_RED,          // Помилка (STA)
    LED_STATE_GREEN_BLINK,  // IP отримано, сервіси запускаються
    LED_STATE_GREEN_SOLID,  // Інтернет OK
    LED_STATE_BLUE_BLINK,   // AP запущена, клієнтів нема
    LED_STATE_BLUE_SOLID    // Клієнт підключений до AP
} wifi_led_status_t;


led_strip_handle_t configure_led(void);


void led_strip_set_color(led_strip_handle_t strip, uint8_t r, uint8_t g, uint8_t b);
void set_led_status(wifi_led_status_t status);
void init_led_status_timer(void);