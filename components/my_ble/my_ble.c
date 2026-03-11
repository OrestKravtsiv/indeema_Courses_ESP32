
#include "my_ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_nimble_hci.h"
#include "esp_bt.h"
#include "nimble/nimble_port.h"

static const char *TAG = "BLE";

// External command handlers from my_proj.c
extern void cmd_set_servo_angle(uint16_t angle);
extern void cmd_set_led_color(uint8_t r, uint8_t g, uint8_t b);

// External telemetry structure
typedef struct {
    float temp_bmp;
    float pressure;
    float temp_aht;
    float humidity;
    float accel_x;
    float accel_y;
    float accel_z;
    uint32_t free_heap;
    uint8_t battery_level;
    uint16_t servo_angle;
    uint8_t led_r;
    uint8_t led_g;
    uint8_t led_b;
    bool led_on;
} telemetry_data_t;

extern telemetry_data_t g_telemetry;


// ============ GLOBAL STATE ============

static uint8_t g_battery_level = CONFIG_BLE_BATTERY_LEVEL;

typedef struct {
    bool led_on;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_status_t;

static led_status_t g_led_status = {
    .led_on = false,
    .r = 0,
    .g = 0,
    .b = 0,
};

static uint16_t g_custom_status_handle = 0;
static uint16_t g_custom_telemetry_handle = 0;
static uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE;

// ============ BATTERY SERVICE ============

static int gatt_access_battery_level(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    // (void)conn_handle;
    // (void)attr_handle;
    // (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return os_mbuf_append(ctxt->om, &g_battery_level, sizeof(g_battery_level)) == 0
                   ? 0
                   : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// ============ CURRENT TIME SERVICE ============

static int gatt_access_current_time(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    uint8_t payload[10] = {0};
    uint16_t year = (uint16_t)(timeinfo.tm_year + 1900);

    // Current Time characteristic format (10 bytes)
    payload[0] = (uint8_t)(year & 0xFF);
    payload[1] = (uint8_t)((year >> 8) & 0xFF);
    payload[2] = (uint8_t)(timeinfo.tm_mon + 1);
    payload[3] = (uint8_t)timeinfo.tm_mday;
    payload[4] = (uint8_t)timeinfo.tm_hour;
    payload[5] = (uint8_t)timeinfo.tm_min;
    payload[6] = (uint8_t)timeinfo.tm_sec;
    payload[7] = (uint8_t)((timeinfo.tm_wday == 0) ? 7 : timeinfo.tm_wday);
    payload[8] = 0; // Fractions256
    payload[9] = 0; // Adjust Reason

    return os_mbuf_append(ctxt->om, payload, sizeof(payload)) == 0
               ? 0
               : BLE_ATT_ERR_INSUFFICIENT_RES;
}

// ============ DEVICE INFORMATION SERVICE ============

static int gatt_access_dis_string(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    const char *str = (const char *)arg;
    return os_mbuf_append(ctxt->om, (const uint8_t *)str, strlen(str)) == 0
               ? 0
               : BLE_ATT_ERR_INSUFFICIENT_RES;
}

// ============ CUSTOM LED SERVICE ============

static void apply_led_state(void)
{
    if (g_led_status.led_on) {
        led_strip_set_color(led_strip, g_led_status.r, g_led_status.g, g_led_status.b);
    } else {
        led_strip_set_color(led_strip, 0, 0, 0);
    }
}

static void ble_notify_status(void)
{
    if (g_conn_handle == BLE_HS_CONN_HANDLE_NONE || g_custom_status_handle == 0) {
        return;
    }

    uint8_t payload[4] = {0};
    payload[0] = g_led_status.led_on ? 1 : 0;
    payload[1] = g_led_status.r;
    payload[2] = g_led_status.g;
    payload[3] = g_led_status.b;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(payload, sizeof(payload));
    if (om != NULL) {
        ble_gatts_notify_custom(g_conn_handle, g_custom_status_handle, om);
    }
}

// ============ TELEMETRY CHARACTERISTIC ============

static int gatt_access_telemetry(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    // Pack telemetry data into binary format (40 bytes)
    // Layout: temp_bmp(4) + pressure(4) + temp_aht(4) + humidity(4) +
    //         accel_x(4) + accel_y(4) + accel_z(4) +
    //         free_heap(4) + servo_angle(2) + led_r(1) + led_g(1) + led_b(1) + led_on(1) + battery(1)
    uint8_t payload[40];
    int offset = 0;
    
    memcpy(&payload[offset], &g_telemetry.temp_bmp, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.pressure, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.temp_aht, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.humidity, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.accel_x, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.accel_y, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.accel_z, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.free_heap, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.servo_angle, 2); offset += 2;
    payload[offset++] = g_telemetry.led_r;
    payload[offset++] = g_telemetry.led_g;
    payload[offset++] = g_telemetry.led_b;
    payload[offset++] = g_telemetry.led_on ? 1 : 0;
    payload[offset++] = g_telemetry.battery_level;
    
    return os_mbuf_append(ctxt->om, payload, sizeof(payload)) == 0
               ? 0
               : BLE_ATT_ERR_INSUFFICIENT_RES;
}

// Notify telemetry data (can be called periodically)
void ble_notify_telemetry(void)
{
    if (g_conn_handle == BLE_HS_CONN_HANDLE_NONE || g_custom_telemetry_handle == 0) {
        return;
    }

    uint8_t payload[40];
    int offset = 0;
    
    memcpy(&payload[offset], &g_telemetry.temp_bmp, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.pressure, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.temp_aht, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.humidity, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.accel_x, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.accel_y, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.accel_z, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.free_heap, 4); offset += 4;
    memcpy(&payload[offset], &g_telemetry.servo_angle, 2); offset += 2;
    payload[offset++] = g_telemetry.led_r;
    payload[offset++] = g_telemetry.led_g;
    payload[offset++] = g_telemetry.led_b;
    payload[offset++] = g_telemetry.led_on ? 1 : 0;
    payload[offset++] = g_telemetry.battery_level;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(payload, sizeof(payload));
    if (om != NULL) {
        ble_gatts_notify_custom(g_conn_handle, g_custom_telemetry_handle, om);
    }
}

// Notify ESP status periodically (lighter notification with just basic status)
void ble_notify_esp_status(void)
{
    if (g_conn_handle == BLE_HS_CONN_HANDLE_NONE || g_custom_status_handle == 0) {
        return;
    }

    // Basic status: free_heap(4) + battery(1) + led_on(1) + servo_angle(2)
    uint8_t payload[8];
    memcpy(&payload[0], &g_telemetry.free_heap, 4);
    payload[4] = g_telemetry.battery_level;
    payload[5] = g_telemetry.led_on ? 1 : 0;
    memcpy(&payload[6], &g_telemetry.servo_angle, 2);

    struct os_mbuf *om = ble_hs_mbuf_from_flat(payload, sizeof(payload));
    if (om != NULL) {
        ble_gatts_notify_custom(g_conn_handle, g_custom_status_handle, om);
    }
}

static int gatt_access_custom_status(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint8_t payload[4] = {0};
    payload[0] = g_led_status.led_on ? 1 : 0;
    payload[1] = g_led_status.r;
    payload[2] = g_led_status.g;
    payload[3] = g_led_status.b;

    return os_mbuf_append(ctxt->om, payload, sizeof(payload)) == 0
               ? 0
               : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int gatt_access_custom_cmd(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    ESP_LOGI(TAG, "Custom LED command received, length=%d", OS_MBUF_PKTLEN(ctxt->om));
    uint8_t buf[4] = {0};
    os_mbuf_copydata(ctxt->om, 0, sizeof(buf), buf);

    uint8_t opcode = buf[0];
    ESP_LOGI(TAG, "BLE Command received: 0x%02X", opcode);

    switch (opcode) {
        case 0x01: // LED ON
            g_led_status.led_on = true;
            ESP_LOGI(TAG, "BLE CMD: LED ON");
            break;
        case 0x02: // LED OFF
            g_led_status.led_on = false;
            ESP_LOGI(TAG, "BLE CMD: LED OFF");
            break;
        case 0x03: // SET COLOR
            g_led_status.led_on = true;
            g_led_status.r = buf[1];
            g_led_status.g = buf[2];
            g_led_status.b = buf[3];
            cmd_set_led_color(buf[1], buf[2], buf[3]);
            ESP_LOGI(TAG, "BLE CMD: LED COLOR RGB(%u, %u, %u)", buf[1], buf[2], buf[3]);
            break;
        case 0x04: // SET SERVO ANGLE
            {
                uint16_t angle = (buf[1] << 8) | buf[2];
                cmd_set_servo_angle(angle);
                ESP_LOGI(TAG, "BLE CMD: SERVO ANGLE %u", angle);
            }
            break;
        default:
            ESP_LOGW(TAG, "BLE CMD: Unknown opcode 0x%02X", opcode);
            return BLE_ATT_ERR_UNLIKELY;
    }

    apply_led_state();
    ble_notify_status();
    return 0;
}

// ============ GATT SERVICES DEFINITION ============

static const struct ble_gatt_svc_def gatt_svcs[] = {
    // Battery Service
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BATTERY_SVC_UUID),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(BATTERY_LEVEL_UUID),
                .access_cb = gatt_access_battery_level,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {0}
        },
    },
    // Current Time Service
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(CTS_SVC_UUID),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(CURRENT_TIME_UUID),
                .access_cb = gatt_access_current_time,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {0}
        },
    },
    // Device Information Service
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(DIS_SVC_UUID),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(MANUFACTURER_NAME_UUID),
                .access_cb = gatt_access_dis_string,
                .arg = (void *)CONFIG_BLE_DIS_MANUFACTURER,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(MODEL_NUMBER_UUID),
                .access_cb = gatt_access_dis_string,
                .arg = (void *)CONFIG_BLE_DIS_MODEL_NUMBER,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(SERIAL_NUMBER_UUID),
                .access_cb = gatt_access_dis_string,
                .arg = (void *)CONFIG_BLE_DIS_SERIAL_NUMBER,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {0}
        },
    },
    // Custom LED Control Service
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &g_custom_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &g_custom_cmd_uuid.u,
                .access_cb = gatt_access_custom_cmd,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = &g_custom_status_uuid.u,
                .access_cb = gatt_access_custom_status,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &g_custom_status_handle,
            },
            {
                .uuid = &g_custom_telemetry_uuid.u,
                .access_cb = gatt_access_telemetry,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &g_custom_telemetry_handle,
            },
            {0}
        },
    },
    {0}
};

// ============ GAP EVENT HANDLING ============

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                g_conn_handle = event->connect.conn_handle;
                ESP_LOGI(TAG, "BLE connected, handle=%d", event->connect.conn_handle);
            } else {
                g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
                ESP_LOGW(TAG, "BLE connect failed; status=%d", event->connect.status);
            }
            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            ESP_LOGI(TAG, "BLE disconnected; reason=%d", event->disconnect.reason);
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "Advertising stopped, restarting...");
            return 0;

        default:
            return 0;
    }
}

// ============ ADVERTISING ============

static void ble_app_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields adv_fields;
    struct ble_hs_adv_fields rsp_fields;

    // Main advertising packet: Flags + UUID only (fits in 31 bytes)
    memset(&adv_fields, 0, sizeof(adv_fields));
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.uuids128 = &g_custom_svc_uuid;
    adv_fields.num_uuids128 = 1;
    adv_fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    // Scan response: Device name (visible when scanning)
    memset(&rsp_fields, 0, sizeof(rsp_fields));
    const char *name = ble_svc_gap_device_name();
    if (name && strlen(name) > 0) {
        rsp_fields.name = (uint8_t *)name;
        rsp_fields.name_len = strlen(name);
        rsp_fields.name_is_complete = 1;
        ESP_LOGI(TAG, "Scan response name: %s (len=%d)", name, rsp_fields.name_len);
    } else {
        const char *fallback = "ESP32-S3";
        rsp_fields.name = (uint8_t *)fallback;
        rsp_fields.name_len = strlen(fallback);
        rsp_fields.name_is_complete = 1;
        ESP_LOGW(TAG, "Using fallback name: %s", fallback);
    }

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_rsp_set_fields failed: %d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 100;  // 62.5 ms
    adv_params.itvl_max = 160;  // 100 ms

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, 
                           ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "BLE advertising started successfully");
}

// ============ HOST SYNCHRONIZATION ============

static void ble_app_on_sync(void)
{
    uint8_t addr_type;
    ble_hs_id_infer_auto(0, &addr_type);
    
    // Ensure device name is set with configured value
    #ifdef CONFIG_BLE_DEVICE_NAME
    ble_svc_gap_device_name_set(CONFIG_BLE_DEVICE_NAME);
    ESP_LOGI(TAG, "Device name set to: %s", CONFIG_BLE_DEVICE_NAME);
    #else
    ESP_LOGW(TAG, "CONFIG_BLE_DEVICE_NAME not defined, using default name");
    #endif
    
    ble_app_advertise();
}

static void ble_host_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
}

// ============ INITIALIZATION ============

esp_err_t ble_init(void)
{
    ESP_LOGI(TAG, "Initializing NimBLE...");

    // nimble_port_init() internally calls esp_bt_controller_init/enable
    // Do NOT call those manually — doing so causes ESP_ERR_INVALID_STATE
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NimBLE port init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "NimBLE host initialized");

    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_hs_cfg.sync_cb = ble_app_on_sync;

    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return ESP_FAIL;
    }

    nimble_port_freertos_init(ble_host_task);
    ESP_LOGI(TAG, "NimBLE initialization complete!");
    return ESP_OK;
}