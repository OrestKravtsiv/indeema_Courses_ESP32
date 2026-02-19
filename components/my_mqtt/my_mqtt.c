
#include "my_mqtt.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "my_led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Forward declarations for MQTT command handlers from my_proj.c
extern void mqtt_set_mode(int mode);
extern void mqtt_set_led_color(int r, int g, int b);
extern void mqtt_get_status(void);

// MQTT Command Queue
typedef struct {
    char topic[64];
    char payload[256];
} mqtt_command_t;

static QueueHandle_t mqtt_command_queue = NULL;

// WiFi state check (declared in my_wifi.h, used here to avoid reconnect when no IP)
extern EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT  BIT0

struct mqtt_status_t {
    bool connected;
    bool reconnect_enabled;
    uint32_t reconnect_delay_ms;
    // Add more fields as needed (e.g., last error, message queue, etc.)
} mqtt_status = {
    .connected = false,
    .reconnect_enabled = true,
    .reconnect_delay_ms = 1000  // Start with 1 second backoff
};



void mqtt_task(void *pvParameters)
{
    mqtt_command_t cmd;
    
    // Create command queue if it doesn't exist
    if (mqtt_command_queue == NULL) {
        mqtt_command_queue = xQueueCreate(10, sizeof(mqtt_command_t));
    }

    while (1) {
        // Process commands from queue
        if (xQueueReceive(mqtt_command_queue, &cmd, pdMS_TO_TICKS(1000))) {
            ESP_LOGI(TAG, "Processing command from: %s", cmd.topic);
            
            // Parse JSON payload
            cJSON *json = cJSON_Parse(cmd.payload);
            if (json == NULL) {
                ESP_LOGE(TAG, "Failed to parse JSON payload");
                continue;
            }

            // Extract command type
            cJSON *cmd_type = cJSON_GetObjectItem(json, "cmd");
            if (cmd_type && cmd_type->valuestring) {
                ESP_LOGI(TAG, "Command: %s", cmd_type->valuestring);

                // Handle different commands
                if (strcmp(cmd_type->valuestring, "mode") == 0) {
                    cJSON *mode = cJSON_GetObjectItem(json, "value");
                    if (mode && mode->valueint >= 0) {
                        mqtt_set_mode(mode->valueint);
                    }
                }
                else if (strcmp(cmd_type->valuestring, "led_color") == 0) {
                    cJSON *r = cJSON_GetObjectItem(json, "r");
                    cJSON *g = cJSON_GetObjectItem(json, "g");
                    cJSON *b = cJSON_GetObjectItem(json, "b");
                    
                    if (r && g && b) {
                        mqtt_set_led_color(r->valueint, g->valueint, b->valueint);
                    }
                }
                else if (strcmp(cmd_type->valuestring, "status") == 0) {
                    mqtt_get_status();
                }
            }
            cJSON_Delete(json);
        }
    }
}

static esp_mqtt_client_handle_t mqtt_client = NULL;


esp_err_t mqtt_publish_data(const char *topic, const char *payload, int qos, bool retain)
{
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!mqtt_status.connected) {
        ESP_LOGW(TAG, "MQTT not connected, cannot publish");
        return ESP_ERR_INVALID_STATE;
    }
    
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, qos, retain ? 1 : 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish to topic %s", topic);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Published to %s with msg_id=%d", topic, msg_id);
    return ESP_OK;
}

esp_err_t mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.hivemq.com:1883",
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_LOGI(TAG, "MQTT client initialized");

    esp_err_t err = esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, esp_mqtt_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "MQTT client started");
    return ESP_OK;
}

static void esp_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch (event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
            break;

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt_status.connected = true;
            
            // Publish online status (retained)
            mqtt_publish_data("esp-lection/status", "Orest_online", 1, true);
            
            // Subscribe to command topic
            msg_id = esp_mqtt_client_subscribe(client, "esp-lection/cmd", 0);
            ESP_LOGI(TAG, "Subscribed to esp-lection/cmd with msg_id=%d", msg_id);
            
            // Start the MQTT task for command processing
            xTaskCreate(mqtt_task, "mqtt_task", 4096, (void *)client, 5, NULL);
            ESP_LOGI(TAG, "MQTT task started");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            mqtt_status.connected = false;
            
            // Get WiFi connection status before attempting reconnect
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                // WiFi is connected, schedule reconnect with backoff
                ESP_LOGI(TAG, "WiFi connected - scheduling MQTT reconnect with backoff");
                vTaskDelay(pdMS_TO_TICKS(2000)); // 2 second backoff for first attempt
                esp_mqtt_client_reconnect(client);
            } else {
                // WiFi not connected, wait for WiFi to recover
                ESP_LOGW(TAG, "WiFi not connected - deferring MQTT reconnect until WiFi is up");
                // The reconnect will be triggered by WiFi event handler when connection is restored
            }
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            
            // Validate topic matches our expected command topic
            if (strncmp(event->topic, "esp-lection/cmd", event->topic_len) == 0) {
                // Queue command for processing in mqtt_task
                mqtt_command_t cmd;
                
                // Copy topic (null-terminate it)
                strncpy(cmd.topic, event->topic, sizeof(cmd.topic) - 1);
                cmd.topic[event->topic_len] = '\0';
                
                // Copy payload (null-terminate it)
                int payload_len = (event->data_len < sizeof(cmd.payload) - 1)
                                  ? event->data_len
                                  : sizeof(cmd.payload) - 1;
                strncpy(cmd.payload, event->data, payload_len);
                cmd.payload[payload_len] = '\0';
                
                // Send to queue for processing
                if (mqtt_command_queue != NULL) {
                    if (!xQueueSend(mqtt_command_queue, &cmd, 0)) {
                        ESP_LOGW(TAG, "Failed to queue MQTT command");
                    }
                } else {
                    ESP_LOGE(TAG, "MQTT command queue not initialized");
                }
            } else {
                ESP_LOGW(TAG, "Received message on unexpected topic");
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x",
                        event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG, "Last tls stack error number: 0x%x",
                        event->error_handle->esp_tls_stack_err);
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "Connection refused, error code: 0x%x",
                        event->error_handle->connect_return_code);
            } else {
                ESP_LOGE(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
            }
            break;

        default:
            ESP_LOGI(TAG, "Other event id: %d", event_id);
            break;
    }
}
