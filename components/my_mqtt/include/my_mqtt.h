#include <stdio.h>
#include "mqtt_client.h"
#include <cJSON.h>



static const char *TAG = "MQTT_CLIENT";


static void esp_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

esp_err_t mqtt_init(void);