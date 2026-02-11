#include "led_strip.h"
#define LED_STRIP_USE_DMA  0

#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)
#define LED_STRIP_MEMORY_BLOCK_WORDS 0 // let the driver choose a proper memory block size automatically

led_strip_handle_t configure_led(void);


void led_strip_test(void);
void led_strip_blink(void);

