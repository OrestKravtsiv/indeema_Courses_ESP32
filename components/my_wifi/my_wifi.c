#include <stdio.h>
#include "my_wifi.h"
#include "my_led.h"
#include "esp_http_client.h"


EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static char  STA_WIFI_SSID[20] = "XXXXXXXXXXXXXXXXXXXXXX"; 
static char  STA_WIFI_PASS[50] = "XXXXXXXXXXXXXXXXXXXXXX";

#define TAG "APP"

static httpd_handle_t server_handle = NULL;
static int ap_connected_clients = 0;

// Check internet connectivity by pinging Google's connectivity check endpoint
void check_internet_connectivity(void) {
    esp_http_client_config_t config = {
        .url = "http://clients3.google.com/generate_204",
        .timeout_ms = 5000,
        .method = HTTP_METHOD_GET,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    
    if (err == ESP_OK && status == 204) {
        ESP_LOGI(TAG, "Internet connectivity: OK");
        set_led_status(LED_STATE_GREEN_SOLID);
    } else {
        ESP_LOGW(TAG, "Internet connectivity: FAILED (err=%d, status=%d)", err, status);
        // Keep green blinking if no internet
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    // --- 1. ОБРОБКА ПОДІЙ WIFI (STA ТА AP) ---
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            
            // Режим клієнта (STA)
            case WIFI_EVENT_STA_START:
                set_led_status(LED_STATE_YELLOW);
                esp_wifi_connect();
                ESP_LOGI(TAG, "Wi-Fi stratup, connecting...");
                break;

            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGW(TAG, "Disconected from router. Reason: %d", event->reason);
                
                // Логіка перепідключення
                if (s_retry_num < STA_MAXIMUM_RETRY) {
                    set_led_status(LED_STATE_YELLOW); // Reconnecting
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGI(TAG, "Reconnecting attempt (%d/%d)", s_retry_num, STA_MAXIMUM_RETRY);
                } else {
                    set_led_status(LED_STATE_RED); // Connection failed
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                    ESP_LOGE(TAG, "Unable to reconnect. Reached maximum attempts");
                }
                break;
            }

            // Режим точки доступу (AP)
            case WIFI_EVENT_AP_START:
                ap_connected_clients = 0;
                set_led_status(LED_STATE_BLUE_BLINK);
                ESP_LOGI(TAG, "Access point active");
                start_webserver();
                break;

            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "Access point inactive");
                break;

            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ap_connected_clients++;
                set_led_status(LED_STATE_BLUE_SOLID);
                ESP_LOGI(TAG, "Client connected (MAC: "MACSTR", AID: %d, Total: %d)", MAC2STR(event->mac), event->aid, ap_connected_clients);
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ap_connected_clients--;
                if (ap_connected_clients <= 0) {
                    ap_connected_clients = 0;
                    set_led_status(LED_STATE_BLUE_BLINK);
                } else {
                    set_led_status(LED_STATE_BLUE_SOLID);
                }
                ESP_LOGI(TAG, "Client disconnected (MAC: "MACSTR", AID: %d, Remaining: %d)", MAC2STR(event->mac), event->aid, ap_connected_clients);
                break;
            }
            
            default:
                break;
        }
    } 
    
    // --- 2. ОБРОБКА МЕРЕЖЕВИХ ПОДІЙ (IP) ---
    else if (event_base == IP_EVENT) {
        ESP_LOGI(TAG, "IP event received: %ld", event_id);
        ESP_LOGI(TAG, "Processing IP event!!!");
        switch (event_id) {
            
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI(TAG, "Отримано IP від роутера: " IPSTR, IP2STR(&event->ip_info.ip));
                s_retry_num = 0; // Скидаємо лічильник спроб
                set_led_status(LED_STATE_GREEN_BLINK); // IP received, services starting
                xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                
                // Check internet connectivity after a short delay
                vTaskDelay(pdMS_TO_TICKS(1000));
                check_internet_connectivity();
                break;
            }

            case IP_EVENT_AP_STAIPASSIGNED: {
                ip_event_ap_staipassigned_t* event = (ip_event_ap_staipassigned_t*) event_data;
                ESP_LOGI(TAG, "Призначено IP клієнту нашої точки: " IPSTR, IP2STR(&event->ip));
                break;
            }
        }
    }
}

void wifi_init_combined(wifi_mode_t mode)
{
    // Initialize LED (white = WiFi not started)
    set_led_status(LED_STATE_WHITE);
    
    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    
    // Ініціалізація NVS (потрібна для збереження паролів)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (mode & WIFI_MODE_STA) esp_netif_create_default_wifi_sta();
    if (mode & WIFI_MODE_AP)  esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    // --- ПРАВИЛЬНА ІНІЦІАЛІЗАЦІЯ СТРУКТУРИ ---
    wifi_config_t wifi_config = {0}; // Зануляємо всю структуру

    // Налаштування для STA (підключення до роутера)
    // Використовуємо strlcpy для безпечного копіювання рядків
    strlcpy((char *)wifi_config.sta.ssid, STA_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, STA_WIFI_PASS, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    // Налаштування для AP (ваша точка доступу)
    strlcpy((char *)wifi_config.ap.ssid, "Orest_test", sizeof(wifi_config.ap.ssid));
    strlcpy((char *)wifi_config.ap.password, "qwerty1234", sizeof(wifi_config.ap.password));
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.channel = 1;

    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));

    // Важливо: встановлюємо конфігурацію окремо для кожного інтерфейсу
    if (mode & WIFI_MODE_STA) {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    }
    if (mode & WIFI_MODE_AP) {
        // Якщо ми в режимі AP, нам потрібна саме ap частина структури
        // Оскільки wifi_config_t це union, ми можемо перевикористати ту ж змінну, 
        // але для AP краще заповнити поля заново або мати окрему змінну.
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    }
    
    ESP_ERROR_CHECK(esp_wifi_start());
}

esp_err_t switch_wifi_mode(wifi_mode_t new_mode) {
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);

    // Якщо ми вже в цьому режимі — нічого не робимо
    if (current_mode == new_mode) {
        ESP_LOGI(TAG, "Вже встановлено режим %d", new_mode);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Перемикання режиму: %d -> %d", current_mode, new_mode);

    // 1. Зупиняємо сервер перед зупинкою Wi-Fi
    stop_webserver(); 

    // 2. Скидаємо лічильник спроб підключення
    s_retry_num = 0;

    // 3. Зупиняємо Wi-Fi
    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не вдалося зупинити Wi-Fi: %s", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(500)); // Даємо час на зупинку

    // 4. Створюємо необхідний мережевий інтерфейс
    if (new_mode == WIFI_MODE_STA || new_mode == WIFI_MODE_APSTA) {
        ESP_LOGI(TAG, "Creating STA network interface...");
        esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
        if (sta_netif == NULL) {
            ESP_LOGW(TAG, "STA netif already exists");
        }
    }
    
    if (new_mode == WIFI_MODE_AP || new_mode == WIFI_MODE_APSTA) {
        ESP_LOGI(TAG, "Creating AP network interface...");
        esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
        if (ap_netif == NULL) {
            ESP_LOGW(TAG, "AP netif already exists");
        }
    }

    // 5. Змінюємо режим
    err = esp_wifi_set_mode(new_mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Помилка set_mode: %s", esp_err_to_name(err));
        return err;
    }

    // 6. Запускаємо Wi-Fi знову
    err = esp_wifi_start();
    
    // Примітка: сервер автоматично запуститься через wifi_event_handler,
    // бо там ми прописали start_webserver() на подію WIFI_EVENT_AP_START
    
    return err;
}

void sntp_setup(void){
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK) {
        printf("Failed to update system time within 10s timeout");
    }
}

void print_current_time(void) {
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    time(&now);
    // Встановлюємо часовий пояс для України (Київ)
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
    tzset();

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Поточний час у Києві: %s", strftime_buf);
}


esp_err_t get_handler(httpd_req_t *req) {
    const char* resp_str = "<html><body><form action='/save' method='POST'>"
                           "SSID: <input name='ssid'><br>"
                           "Pass: <input name='pass'><br>"
                           "<input type='submit' value='Connect'></form></body></html>";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

esp_err_t wifi_config_post_handler(httpd_req_t *req) {
    char buf[256]; // Трішки збільшимо буфер для безпеки
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Payload too large");
        return ESP_FAIL;
    }

    // 1. Отримуємо дані з POST запиту
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "Raw POST data: %s", buf);

    // 2. Буфери для сирих та декодованих даних
    char raw_ssid[32] = {0};
    char raw_pass[64] = {0};
    char decoded_ssid[32] = {0};
    char decoded_pass[64] = {0};

    // Витягуємо значення за ключами
    if (httpd_query_key_value(buf, "ssid", raw_ssid, sizeof(raw_ssid)) == ESP_OK &&
        httpd_query_key_value(buf, "pass", raw_pass, sizeof(raw_pass)) == ESP_OK) {
        
        // ДЕКОДУВАННЯ: перетворюємо "+" на пробіли та обробляємо спецсимволи
        url_decode(decoded_ssid, raw_ssid);
        url_decode(decoded_pass, raw_pass);

        ESP_LOGI(TAG, "Decoded SSID: [%s]", decoded_ssid);
        ESP_LOGI(TAG, "Decoded Password: [%s]", decoded_pass);

            
        // Відправляємо відповідь користувачу
        const char *resp_str = "<html><body><h1>Settings saved!</h1><p>ESP32 is switching to STA mode and connecting to your router...</p></body></html>";
        httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);


        // 5. Затримка, щоб сервер встиг відправити пакет, і зміна режиму
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // 1. Формуємо нову конфігурацію WiFi
        wifi_config_t wifi_config = {0};
        strncpy((char *)wifi_config.sta.ssid, decoded_ssid, sizeof(wifi_config.sta.ssid));
        strncpy((char *)wifi_config.sta.password, decoded_pass, sizeof(wifi_config.sta.password));
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        // 2. Reset retry counter
        s_retry_num = 0;
        
        // 3. Зупиняємо WiFi перед зміною конфігурації
        ESP_LOGI(TAG, "Stopping WiFi...");
        esp_wifi_stop();
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // 4. Створюємо STA мережевий інтерфейс (якщо його немає)
        ESP_LOGI(TAG, "Creating STA network interface...");
        esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
        if (sta_netif == NULL) {
            ESP_LOGW(TAG, "STA netif already exists or creation failed");
        }
        
        // 5. Зміняємо режим на STA
        ESP_LOGI(TAG, "Setting STA mode...");
        esp_wifi_set_mode(WIFI_MODE_STA);
        
        // 6. Встановлюємо нові credentials
        ESP_LOGI(TAG, "Setting new credentials...");
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        
        // 6. Запускаємо WiFi (це викличе WIFI_EVENT_STA_START -> esp_wifi_connect)
        ESP_LOGI(TAG, "Starting WiFi with new credentials...");
        esp_wifi_start();
        
        ESP_LOGI(TAG, "WiFi reconfiguration complete, waiting for connection...");


        
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID or Password");
    }

    return ESP_OK;
}


// Функція, яка запускає сервер
void start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Запускаємо сервер
    if (httpd_start(&server, &config) == ESP_OK) {
        
        // 1. Описуємо URI для GET
        httpd_uri_t uri_get = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = get_handler, // функція, яку ми створили раніше
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get);

        // 2. Описуємо URI для POST
        httpd_uri_t uri_post = {
            .uri      = "/save",
            .method   = HTTP_POST,
            .handler  = wifi_config_post_handler, // функція обробки POST
            .user_ctx = NULL
        };
        // ТЕПЕР це спрацює, бо ми всередині функції
        httpd_register_uri_handler(server, &uri_post);
        
        ESP_LOGI(TAG, "Server started on port: '%d'", config.server_port);
    }
}

void stop_webserver(void) {
    if (server_handle != NULL) {
        if (httpd_stop(server_handle) == ESP_OK) {
            server_handle = NULL; // Обов'язково зануляємо після зупинки!
            ESP_LOGI(TAG, "Web server stopped");
        }
    }
}


void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}