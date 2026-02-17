
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
#include "esp_netif_sntp.h"
#include <esp_http_server.h>



// --- НАЛАШТУВАННЯ AP (Точка доступу) ---
#define WIFI_SSID "Orest_test"
#define WIFI_PASS "qwerty1234"
#define ESP_WIFI_CHANNEL   1
#define MAX_STA_CONN       4

// --- НАЛАШТУВАННЯ STA (Роутер) ---
#define STA_MAXIMUM_RETRY  5

// Глобальні змінні
extern EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1



static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);

void wifi_init_combined(wifi_mode_t mode);

esp_err_t switch_wifi_mode(wifi_mode_t new_mode);

void check_internet_connectivity(void);

void sntp_setup(void);

void print_current_time(void);

esp_err_t get_handler(httpd_req_t *req);



esp_err_t wifi_config_post_handler(httpd_req_t *req);



void start_webserver(void);

void stop_webserver(void);


void url_decode(char *dst, const char *src);