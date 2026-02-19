
#include "my_mqtt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void mqtt_task(void *pvParameters)
{
    while (1) {
        // Placeholder for periodic MQTT tasks, e.g., telemetry publishing
        vTaskDelay(pdMS_TO_TICKS(10000)); // Publish every 10 seconds
    }
}

static esp_mqtt_client_handle_t mqtt_client = NULL;


struct mqtt_status_t {
    bool connected;
    // Add more fields as needed (e.g., last error, message queue, etc.)
} mqtt_status = {
    .connected = false
};


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
        /*MQTT_EVENT_CONNECTED
            Publish status = "online" (often retained).
            Subscribe to: esp-lection/cmd
            Start the periodic telemetry task (timer/task).*/
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt_status.connected = true;
            msg_id = esp_mqtt_client_publish(client, "esp-lection/status", "Orest_online", 0, 0, 0); 
            ESP_LOGI(TAG, "Published with msg_id=%d", msg_id);//Publish status = "online" (often retained).
            msg_id = esp_mqtt_client_subscribe(client, "esp-lection/cmd", 0);
            ESP_LOGI(TAG, "Subscribed with msg_id=%d", msg_id); //Subscribe to: esp-lection/cmd
            xTaskCreate(mqtt_task, "mqtt_task", 4096, NULL, 5, NULL);
            ESP_LOGI(TAG, "MQTT task started");//Start the periodic telemetry task (timer/task).
            break;
        /*MQTT_EVENT_DATA
            Parse the topic and payload (using cJSON or a lightweight parser).
            Validate:
                topic match
                payload schema/version
            Execute the command in a separate task (not inside the event handler), e.g.: via a queue / task 
            notification
            */
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            
            break;
        case MQTT_EVENT_DISCONNECTED:
        /*Mark the state as “MQTT down”.
        Stop/pause telemetry publishing (or buffer it).
        Start reconnect using backoff
        Do not reconnect if Wi-Fi has no IP yet (let the Wi-Fi manager recover first).*/
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            mqtt_status.connected = false;
            
    }
//to do for later

}
