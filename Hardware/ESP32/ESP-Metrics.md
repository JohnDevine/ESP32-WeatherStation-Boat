# ESP32 System Metrics Available via ESP-IDF

This document outlines key metrics and telemetry data that can be obtained from an ESP32 processor using the ESP-IDF framework.

## System Performance & Resource Metrics

| Metric | Description | API/Component |
|--------|-------------|---------------|
| CPU Frequency | Current clock frequency of the CPU cores in MHz | `esp_clk_cpu_freq()` |
| CPU Temperature | Internal temperature sensor reading in Celsius | `temp_sensor_get_celsius()` |
| Free Heap | Available heap memory in bytes | `esp_get_free_heap_size()` |
| Min Free Heap | Minimum free heap size since boot | `esp_get_minimum_free_heap_size()` |
| Task Runtime Stats | CPU time used by each FreeRTOS task | `vTaskGetRunTimeStats()` |
| Task Stack High Water Mark | Closest a task has come to stack overflow | `uxTaskGetStackHighWaterMark()` |
| Task Priority | Priority level of running tasks | `uxTaskPriorityGet()` |
| Reset Reason | Cause of the last reset/reboot | `esp_reset_reason()` |
| Uptime | Time since last boot in milliseconds | `esp_timer_get_time() / 1000` |

## Power Management

| Metric | Description | API/Component |
|--------|-------------|---------------|
| Power Mode | Current power saving mode | `esp_pm_get_configuration()` |
| Light Sleep Duration | Time spent in light sleep mode | Custom counter with `esp_sleep_enable_timer_wakeup()` |
| Deep Sleep Duration | Time spent in deep sleep mode | RTC timer via `esp_sleep_get_wakeup_cause()` |
| VDD33 Voltage | Supply voltage level (3.3V rail) | ADC with calibration or power monitoring IC |
| Current Consumption | Real-time current draw (requires external measurement) | INA219 or similar external sensor |

## Connectivity Metrics

| Metric | Description | API/Component |
|--------|-------------|---------------|
| WiFi RSSI | Signal strength of connected WiFi network | `esp_wifi_sta_get_rssi()` |
| WiFi TX Power | Transmission power level of WiFi radio | `esp_wifi_get_max_tx_power()` |
| WiFi Tx/Rx Bytes | Data transmitted/received over WiFi | `esp_wifi_get_statistics()` |
| BT/BLE RSSI | Signal strength of Bluetooth connections | `esp_bt_gap_read_rssi_delta()` |
| BT/BLE Connected Devices | Number and details of connected devices | Various BT/BLE APIs |
| IP Address | Current IP address information | `esp_netif_get_ip_info()` |
| Network Speed | Current network connection speed | Custom measurements using `esp_timer` |

## Storage & Peripheral Metrics

| Metric | Description | API/Component |
|--------|-------------|---------------|
| Flash Usage | Used/available flash storage space | `esp_partition_get_size()` |
| Flash R/W Operations | Count of flash read/write operations | Custom counters in `esp_partition` calls |
| SPIFFS/FAT Usage | Filesystem space utilization | `esp_spiffs_info()` or similar |
| I2C Bus Errors | Error counts for I2C communications | Custom error tracking in I2C driver |
| SPI Performance | Transaction speed/throughput for SPI | Custom timing with `esp_timer` |
| GPIO Status | Current state of GPIO pins | `gpio_get_level()` |

## Hardware-Specific Metrics

| Metric | Description | API/Component |
|--------|-------------|---------------|
| Chip ID | Unique identifier for the ESP32 chip | `esp_efuse_mac_get_default()` |
| Chip Revision | Silicon revision of the chip | `esp_efuse_get_chip_ver()` |
| Flash Chip Size | Size of attached flash memory | `spi_flash_get_chip_size()` |
| MAC Address | Factory-assigned MAC address | `esp_read_mac()` |
| Core Count | Number of available CPU cores | `esp_chip_info()` |

## Application-Specific Metrics

| Metric | Description | API/Component |
|--------|-------------|---------------|
| Boot Count | Number of times the device has booted | Custom implementation using NVS |
| Crash Count | Number of unexpected resets | Custom implementation with reset reason |
| OTA Update Status | Status of over-the-air firmware updates | `esp_ota_get_state_partition()` |
| Last Update Time | Timestamp of last firmware update | Custom implementation using RTC |
| App-Specific Timers | Custom timing metrics for application logic | `esp_timer` APIs |

## Collecting and Using Metrics

These metrics can be:

1. Logged locally for debugging
2. Sent to a server for remote monitoring
3. Used for dynamic behavior adjustment
4. Displayed on attached screens/LCDs
5. Analyzed for predictive maintenance

For production applications, consider implementing a metrics collection system that:
- Samples at appropriate intervals to avoid performance impact
- Buffers data to minimize storage writes
- Compresses data before transmission
- Provides local alerting for critical thresholds

## References

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
