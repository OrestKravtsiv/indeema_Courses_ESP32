
// WIFI
#include "esp_system.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_log.h"



// --- НАЛАШТУВАННЯ AP (Точка доступу) ---
#define WIFI_SSID "Orest_test"
#define WIFI_PASS "qwerty1234"
#define ESP_WIFI_CHANNEL   1
#define MAX_STA_CONN       4

// --- НАЛАШТУВАННЯ STA (Роутер) ---
#define STA_WIFI_SSID  "IGOR"    
#define STA_WIFI_PASS  "30031988"
#define STA_MAXIMUM_RETRY  5

// Глобальні змінні
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1



static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);

void wifi_init_combined(void);
