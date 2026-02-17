## Project Overview
ESP32 multi-mode WiFi system with web configuration portal, joystick control, and LED status indicators. Built with FreeRTOS to monitor task performance, manage WiFi modes (AP/STA), and provide real-time system diagnostics.

### Features
- **WiFi Management**: Switch between AP and STA modes dynamically
- **Web Configuration**: Configure WiFi credentials via captive portal
- **LED Status Indicators**: Visual feedback for connection states
- **Joystick Control**: Hardware input for mode switching
- **Real-time Monitoring**: Track CPU usage, task status, and core assignment
- **SNTP Time Sync**: Automatic time synchronization

### FreeRTOS Configuration
Enable these settings in sdkconfig:

- `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y`
- `CONFIG_FREERTOS_USE_TRACE_FACILITY=y`
- `CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=y`
- `CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID=y`

### System Functionality
- **System Status**: Logs every 5 seconds
- **Task List**: Shows name, state, priority, and core ID
- **CPU Load**: Displays execution time and percentage for each task
- **Free Heap**: Monitor available memory

### WiFi Modes
- **AP Mode** (Access Point): Creates "Orest_test" network for configuration
- **STA Mode** (Station): Connects to your router with saved credentials


