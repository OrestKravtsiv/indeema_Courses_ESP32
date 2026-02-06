Project Overview
A real-time system monitor for ESP32 using FreeRTOS to track task status, CPU usage, and core assignment.

Enable these settings in sdkconfig:

CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y

CONFIG_FREERTOS_USE_TRACE_FACILITY=y

CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=y

CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID=y

Functionality
System Status: Logs every 5 seconds.

Task List: Shows name, state, priority, and core ID.

CPU Load: Displays execution time and percentage for each task.