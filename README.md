## Project Overview
ESP32 multi-mode WiFi system with web configuration portal, joystick control, and LED status indicators. Built with FreeRTOS to monitor task performance, manage WiFi modes (AP/STA), and provide real-time system diagnostics.

### Features
- **WiFi Management**: Switch between AP and STA modes dynamically
- **Web Configuration**: Configure WiFi credentials via captive portal
- **LED Status Indicators**: Visual feedback for connection states
- **Joystick Control**: Hardware input for mode switching
- **Real-time Monitoring**: Track CPU usage, task status, and core assignment
- **SNTP Time Sync**: Automatic time synchronization
- **BLE Peripheral (NimBLE)**: Standard GATT services + custom LED control

### FreeRTOS Configuration
Enable these settings in sdkconfig:

- `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y`
- `CONFIG_FREERTOS_USE_TRACE_FACILITY=y`
- `CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=y`
- `CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID=y`

### BLE GATT Services
**Generic Access Profile (GAP)**: 0x1800

**Standard Services**
- Battery Service (BAS): 0x180F, Battery Level (0x2A19)
- Current Time Service (CTS): 0x1805, Current Time (0x2A2B)
- Device Information Service (DIS): 0x180A, Manufacturer Name (0x2A29), Model Number (0x2A24), Serial Number (0x2A25)

**Custom Service**
- Service UUID: 12345678-1234-5678-1234-56789abcdef0
- Command Characteristic UUID: 12345678-1234-5678-1234-56789abcdef1 (Write/Write No Resp)
- Status Characteristic UUID: 12345678-1234-5678-1234-56789abcdef2 (Read/Notify)

#### Command Protocol (Custom Command Characteristic)
- `0x01` = LED ON
- `0x02` = LED OFF
- `0x03 R G B` = Set LED color (bytes 1..3 are RGB)

#### Status Payload (Custom Status Characteristic)
- Byte0: LED on/off (0/1)
- Byte1: R
- Byte2: G
- Byte3: B

### System Functionality
- **System Status**: Logs every 5 seconds
- **Task List**: Shows name, state, priority, and core ID
- **CPU Load**: Displays execution time and percentage for each task
- **Free Heap**: Monitor available memory

### WiFi Modes
- **AP Mode** (Access Point): Creates "Orest_test" network for configuration
- **STA Mode** (Station): Connects to your router with saved credentials


