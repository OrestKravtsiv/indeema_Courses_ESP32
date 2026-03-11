#pragma once

#include "esp_err.h"
#include <string.h>
#include <time.h>

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"

#include "my_led.h"

#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_nimble_hci.h"

#ifdef __cplusplus
extern "C" {
#endif


// ============ UUID DEFINITIONS ============

// Battery Service (0x180F)
#define BATTERY_SVC_UUID        0x180F
#define BATTERY_LEVEL_UUID      0x2A19

// Current Time Service (0x1805)
#define CTS_SVC_UUID            0x1805
#define CURRENT_TIME_UUID       0x2A2B

// Device Information Service (0x180A)
#define DIS_SVC_UUID            0x180A
#define MANUFACTURER_NAME_UUID  0x2A29
#define MODEL_NUMBER_UUID       0x2A24
#define SERIAL_NUMBER_UUID      0x2A25

// Custom Service (LED Control)
// Service UUID: 12345678-1234-5678-1234-56789abcdef0
static const ble_uuid128_t g_custom_svc_uuid = BLE_UUID128_INIT(
    0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
    0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0x78, 0x12);

// Command Characteristic UUID: 12345678-1234-5678-1234-56789abcdef1
static const ble_uuid128_t g_custom_cmd_uuid = BLE_UUID128_INIT(
    0xf1, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
    0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0x78, 0x12);

// Status Characteristic UUID: 12345678-1234-5678-1234-56789abcdef2
static const ble_uuid128_t g_custom_status_uuid = BLE_UUID128_INIT(
    0xf2, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
    0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0x78, 0x12);

// Telemetry Characteristic UUID: 12345678-1234-5678-1234-56789abcdef3
static const ble_uuid128_t g_custom_telemetry_uuid = BLE_UUID128_INIT(
    0xf3, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
    0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0x78, 0x12);


esp_err_t ble_init(void);
void ble_notify_esp_status(void);
void ble_notify_telemetry(void);

#ifdef __cplusplus
}

#endif