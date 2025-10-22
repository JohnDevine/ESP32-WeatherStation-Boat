/**
 * @file SystemMetrics.c
 * @brief ESP32 System Metrics Library Implementation
 * 
 * @version 1.0.0
 * @date 2025-10-17
 * @author John Devine
 * @email john.h.devine@gmail.com
 * @copyright Copyright (c) 2025 John Devine
 */

// =============================
// Function Prototypes
// =============================
static const char* format_cpu_frequency(void);
static const char* format_cpu_temperature(void);
static const char* format_free_heap(void);
static const char* format_min_free_heap(void);
static const char* format_uptime(void);
static const char* format_reset_reason(void);
static const char* format_task_runtime_stats(void);
static const char* format_task_priority(void);
static const char* format_power_mode(void);
static const char* format_light_sleep_duration(void);
static const char* format_deep_sleep_duration(void);
static const char* format_vdd33_voltage(void);
static const char* format_current_consumption(void);
static const char* format_wifi_rssi(void);
static const char* format_wifi_tx_power(void);
static const char* format_wifi_tx_rx_bytes(void);
static const char* format_ip_address(void);
static const char* format_wifi_status(void);
static const char* format_network_speed(void);
static const char* format_bt_ble_rssi(void);
static const char* format_bt_ble_connected_devices(void);
static const char* format_flash_usage(void);
static const char* format_flash_rw_operations(void);
static const char* format_spiffs_usage(void);
static const char* format_i2c_bus_errors(void);
static const char* format_spi_performance(void);
static const char* format_gpio_status(void);
static const char* format_chip_id(void);
static const char* format_mac_address(void);
static const char* format_flash_size(void);
static const char* format_chip_revision(void);
static const char* format_core_count(void);
static const char* format_task_count(void);
static const char* format_task_stack_hwm(void);
static const char* format_boot_count(void);
static const char* format_crash_count(void);
static const char* format_ota_update_status(void);
static const char* format_last_update_time(void);
static const char* format_app_specific_timers(void);

// =============================
// Includes
// =============================
#include "SystemMetrics.h"
#include "../../../include/version.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <stdbool.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_ota_ops.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_spiffs.h"
#include "esp_partition.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/temperature_sensor.h"
#ifdef CONFIG_BT_ENABLED
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#endif
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// =============================
// Constants & Definitions
// =============================
static const char *TAG = "SYSTEM_METRICS";

// Register SystemMetrics library version
REGISTER_VERSION(SystemMetrics, "1.0.0", "2025-10-18");

#define SYSTEM_METRICS_VERSION "1.0.0"
#define SYSTEM_METRICS_BUILD_DATE __DATE__

// Static buffer for metric strings
static char metric_buffer[METRIC_MAX_STRING_LENGTH];

// Last error code
static metric_error_t last_error = METRIC_OK;

// Temperature sensor handle
static temperature_sensor_handle_t temp_sensor = NULL;

// ADC handle for voltage measurement
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;

// NVS handles for persistent counters
static nvs_handle_t metrics_nvs_handle = 0;

// =============================
// Function Definitions
// =============================

bool system_metrics_init(void)
{
    ESP_LOGI(TAG, "Initializing System Metrics Library v%s", SYSTEM_METRICS_VERSION);
    
    // Initialize NVS for persistent counters
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Open NVS handle
    ret = nvs_open("metrics", NVS_READWRITE, &metrics_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed: %s", esp_err_to_name(ret));
    }
    
    // Initialize temperature sensor (only on supported ESP32 variants)
#if defined(CONFIG_IDF_TARGET_ESP32S2) \
 || defined(CONFIG_IDF_TARGET_ESP32S3) \
 || defined(CONFIG_IDF_TARGET_ESP32C2) \
 || defined(CONFIG_IDF_TARGET_ESP32C3) \
 || defined(CONFIG_IDF_TARGET_ESP32C6)
    temperature_sensor_config_t temp_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    ret = temperature_sensor_install(&temp_config, &temp_sensor);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Temperature sensor initialization failed: %s", esp_err_to_name(ret));
        temp_sensor = NULL;
    } else {
        ret = temperature_sensor_enable(temp_sensor);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Temperature sensor enable failed: %s", esp_err_to_name(ret));
            temperature_sensor_uninstall(temp_sensor);
            temp_sensor = NULL;
        } else {
            ESP_LOGI(TAG, "Temperature sensor initialized successfully");
        }
    }
#else
    // Original ESP32 - no built-in temperature sensor
    temp_sensor = NULL;
    ESP_LOGI(TAG, "Temperature sensor not available on ESP32 (use external sensor)");
#endif
    
    // Initialize ADC for voltage measurement
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC unit initialization failed: %s", esp_err_to_name(ret));
        adc_handle = NULL;
    } else {
        // Configure ADC channel for VDD33 measurement (if available)
        adc_oneshot_chan_cfg_t config = {
            .bitwidth = ADC_BITWIDTH_DEFAULT,
            .atten = ADC_ATTEN_DB_12,
        };
        adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_0, &config);
        
        // Initialize calibration
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ADC calibration failed: %s", esp_err_to_name(ret));
            adc_cali_handle = NULL;
        }
    }
    
    // Update boot count
    if (metrics_nvs_handle != 0) {
        uint32_t boot_count = 0;
        size_t required_size = sizeof(boot_count);
        nvs_get_blob(metrics_nvs_handle, "boot_count", &boot_count, &required_size);
        boot_count++;
        nvs_set_blob(metrics_nvs_handle, "boot_count", &boot_count, sizeof(boot_count));
        nvs_commit(metrics_nvs_handle);
        ESP_LOGI(TAG, "Boot count: %lu", boot_count);
    }
    
    ESP_LOGI(TAG, "System Metrics Library initialized");
    return true;
}

const char* get_system_metric(system_metric_t metric)
{
    last_error = METRIC_OK;
    
    if (metric >= METRIC_COUNT) {
        last_error = METRIC_ERROR_INVALID_ID;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Invalid metric ID (%d)", metric);
        return metric_buffer;
    }
    
    switch (metric) {
        case METRIC_CPU_FREQUENCY:
            return format_cpu_frequency();
        case METRIC_CPU_TEMPERATURE:
            return format_cpu_temperature();
        case METRIC_FREE_HEAP:
            return format_free_heap();
        case METRIC_MIN_FREE_HEAP:
            return format_min_free_heap();
        case METRIC_UPTIME:
            return format_uptime();
        case METRIC_RESET_REASON:
            return format_reset_reason();
        case METRIC_TASK_RUNTIME_STATS:
            return format_task_runtime_stats();
        case METRIC_TASK_PRIORITY:
            return format_task_priority();
        case METRIC_POWER_MODE:
            return format_power_mode();
        case METRIC_LIGHT_SLEEP_DURATION:
            return format_light_sleep_duration();
        case METRIC_DEEP_SLEEP_DURATION:
            return format_deep_sleep_duration();
        case METRIC_VDD33_VOLTAGE:
            return format_vdd33_voltage();
        case METRIC_CURRENT_CONSUMPTION:
            return format_current_consumption();
        case METRIC_WIFI_RSSI:
            return format_wifi_rssi();
        case METRIC_WIFI_TX_POWER:
            return format_wifi_tx_power();
        case METRIC_WIFI_TX_RX_BYTES:
            return format_wifi_tx_rx_bytes();
        case METRIC_IP_ADDRESS:
            return format_ip_address();
        case METRIC_WIFI_STATUS:
            return format_wifi_status();
        case METRIC_NETWORK_SPEED:
            return format_network_speed();
        case METRIC_BT_BLE_RSSI:
            return format_bt_ble_rssi();
        case METRIC_BT_BLE_CONNECTED_DEVICES:
            return format_bt_ble_connected_devices();
        case METRIC_FLASH_USAGE:
            return format_flash_usage();
        case METRIC_FLASH_RW_OPERATIONS:
            return format_flash_rw_operations();
        case METRIC_SPIFFS_USAGE:
            return format_spiffs_usage();
        case METRIC_I2C_BUS_ERRORS:
            return format_i2c_bus_errors();
        case METRIC_SPI_PERFORMANCE:
            return format_spi_performance();
        case METRIC_GPIO_STATUS:
            return format_gpio_status();
        case METRIC_CHIP_ID:
            return format_chip_id();
        case METRIC_MAC_ADDRESS:
            return format_mac_address();
        case METRIC_FLASH_SIZE:
            return format_flash_size();
        case METRIC_CHIP_REVISION:
            return format_chip_revision();
        case METRIC_CORE_COUNT:
            return format_core_count();
        case METRIC_TASK_COUNT:
            return format_task_count();
        case METRIC_TASK_STACK_HWM:
            return format_task_stack_hwm();
        case METRIC_BOOT_COUNT:
            return format_boot_count();
        case METRIC_CRASH_COUNT:
            return format_crash_count();
        case METRIC_OTA_UPDATE_STATUS:
            return format_ota_update_status();
        case METRIC_LAST_UPDATE_TIME:
            return format_last_update_time();
        case METRIC_APP_SPECIFIC_TIMERS:
            return format_app_specific_timers();
        default:
            last_error = METRIC_ERROR_INVALID_ID;
            snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Unimplemented metric (%d)", metric);
            return metric_buffer;
    }
}

metric_error_t get_metric_error(void)
{
    return last_error;
}

const char* get_metric_description(system_metric_t metric)
{
    static const char* descriptions[] = {
        [METRIC_CPU_FREQUENCY] = "CPU clock frequency in MHz",
        [METRIC_CPU_TEMPERATURE] = "Internal temperature sensor in Celsius",
        [METRIC_FREE_HEAP] = "Available heap memory in bytes",
        [METRIC_MIN_FREE_HEAP] = "Minimum free heap size since boot",
        [METRIC_UPTIME] = "Time since boot in milliseconds",
        [METRIC_RESET_REASON] = "Cause of last reset/reboot",
        [METRIC_TASK_RUNTIME_STATS] = "CPU time used by current task",
        [METRIC_TASK_PRIORITY] = "Priority level of current task",
        [METRIC_POWER_MODE] = "Current power saving mode",
        [METRIC_LIGHT_SLEEP_DURATION] = "Total time spent in light sleep mode",
        [METRIC_DEEP_SLEEP_DURATION] = "Total time spent in deep sleep mode",
        [METRIC_VDD33_VOLTAGE] = "Supply voltage level (3.3V rail)",
        [METRIC_CURRENT_CONSUMPTION] = "Current power consumption in microamps",
        [METRIC_WIFI_RSSI] = "WiFi signal strength in dBm",
        [METRIC_WIFI_TX_POWER] = "WiFi transmission power level",
        [METRIC_WIFI_TX_RX_BYTES] = "Data transmitted/received over WiFi",
        [METRIC_IP_ADDRESS] = "Current IP address",
        [METRIC_WIFI_STATUS] = "WiFi connection status",
        [METRIC_NETWORK_SPEED] = "Current network connection speed",
        [METRIC_BT_BLE_RSSI] = "Bluetooth/BLE signal strength in dBm",
        [METRIC_BT_BLE_CONNECTED_DEVICES] = "Number of connected BT/BLE devices",
        [METRIC_FLASH_USAGE] = "Used/available flash storage space",
        [METRIC_FLASH_RW_OPERATIONS] = "Flash read/write operation counters",
        [METRIC_SPIFFS_USAGE] = "SPIFFS filesystem space utilization",
        [METRIC_I2C_BUS_ERRORS] = "I2C bus error count",
        [METRIC_SPI_PERFORMANCE] = "SPI bus performance metrics",
        [METRIC_GPIO_STATUS] = "GPIO pin status and configuration",
        [METRIC_CHIP_ID] = "Unique ESP32 chip identifier",
        [METRIC_MAC_ADDRESS] = "Factory-assigned MAC address",
        [METRIC_FLASH_SIZE] = "Flash memory size in bytes",
        [METRIC_CHIP_REVISION] = "Silicon revision of the chip",
        [METRIC_CORE_COUNT] = "Number of available CPU cores",
        [METRIC_TASK_COUNT] = "Number of active FreeRTOS tasks",
        [METRIC_TASK_STACK_HWM] = "Current task stack high water mark",
        [METRIC_BOOT_COUNT] = "Number of times device has booted",
        [METRIC_CRASH_COUNT] = "Number of unexpected resets",
        [METRIC_OTA_UPDATE_STATUS] = "OTA update status and version",
        [METRIC_LAST_UPDATE_TIME] = "Timestamp of last system update",
        [METRIC_APP_SPECIFIC_TIMERS] = "Application-specific timer values"
    };
    
    if (metric >= METRIC_COUNT) {
        return "Invalid metric";
    }
    
    return descriptions[metric];
}

// =============================
// =============================
// Private Metric Formatters
// =============================

static const char* format_cpu_frequency(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    // Default ESP32 frequencies are 240MHz, 160MHz, 80MHz
    // For simplicity, assume 240MHz as this is most common
    uint32_t freq_mhz = 240; // Could be read from config
    
    snprintf(metric_buffer, sizeof(metric_buffer), "%lu MHz (default)", freq_mhz);
    return metric_buffer;
}

static const char* format_cpu_temperature(void)
{
#if defined(CONFIG_IDF_TARGET_ESP32S2) \
 || defined(CONFIG_IDF_TARGET_ESP32S3) \
 || defined(CONFIG_IDF_TARGET_ESP32C2) \
 || defined(CONFIG_IDF_TARGET_ESP32C3) \
 || defined(CONFIG_IDF_TARGET_ESP32C6)
    // ESP32 variants with built-in temperature sensor
    if (temp_sensor == NULL) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Temperature sensor not initialized");
        return metric_buffer;
    }
    
    float temperature;
    esp_err_t ret = temperature_sensor_get_celsius(temp_sensor, &temperature);
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_HARDWARE_FAULT;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Temperature read failed");
        return metric_buffer;
    }
    
    snprintf(metric_buffer, sizeof(metric_buffer), "%.1f°C", temperature);
    return metric_buffer;
#else
    // Original ESP32 - no built-in temperature sensor
    last_error = METRIC_ERROR_NOT_SUPPORTED;
    snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Temperature sensor not available on ESP32");
    return metric_buffer;
#endif
}

static const char* format_free_heap(void)
{
    uint32_t free_heap = esp_get_free_heap_size();
    snprintf(metric_buffer, sizeof(metric_buffer), "%lu bytes", free_heap);
    return metric_buffer;
}

static const char* format_min_free_heap(void)
{
    uint32_t min_free_heap = esp_get_minimum_free_heap_size();
    snprintf(metric_buffer, sizeof(metric_buffer), "%lu bytes", min_free_heap);
    return metric_buffer;
}

static const char* format_uptime(void)
{
    int64_t uptime_us = esp_timer_get_time();
    uint64_t uptime_ms = uptime_us / 1000;
    
    // Format as days:hours:minutes:seconds.milliseconds
    uint32_t days = uptime_ms / (24 * 60 * 60 * 1000);
    uint32_t hours = (uptime_ms / (60 * 60 * 1000)) % 24;
    uint32_t minutes = (uptime_ms / (60 * 1000)) % 60;
    uint32_t seconds = (uptime_ms / 1000) % 60;
    uint32_t ms = uptime_ms % 1000;
    
    if (days > 0) {
        snprintf(metric_buffer, sizeof(metric_buffer), "%lud %02lu:%02lu:%02lu.%03lu", 
                days, hours, minutes, seconds, ms);
    } else {
        snprintf(metric_buffer, sizeof(metric_buffer), "%02lu:%02lu:%02lu.%03lu", 
                hours, minutes, seconds, ms);
    }
    return metric_buffer;
}

static const char* format_reset_reason(void)
{
    esp_reset_reason_t reason = esp_reset_reason();
    const char* reason_str;
    
    switch (reason) {
        case ESP_RST_UNKNOWN:    reason_str = "Unknown"; break;
        case ESP_RST_POWERON:    reason_str = "Power-on"; break;
        case ESP_RST_EXT:        reason_str = "External reset"; break;
        case ESP_RST_SW:         reason_str = "Software reset"; break;
        case ESP_RST_PANIC:      reason_str = "Software panic"; break;
        case ESP_RST_INT_WDT:    reason_str = "Interrupt watchdog"; break;
        case ESP_RST_TASK_WDT:   reason_str = "Task watchdog"; break;
        case ESP_RST_WDT:        reason_str = "Other watchdog"; break;
        case ESP_RST_DEEPSLEEP:  reason_str = "Deep sleep reset"; break;
        case ESP_RST_BROWNOUT:   reason_str = "Brownout reset"; break;
        case ESP_RST_SDIO:       reason_str = "SDIO reset"; break;
        default:                 reason_str = "Undefined"; break;
    }
    
    snprintf(metric_buffer, sizeof(metric_buffer), "%s (%d)", reason_str, reason);
    return metric_buffer;
}

static const char* format_wifi_rssi(void)
{
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: WiFi not connected");
        return metric_buffer;
    }
    
    snprintf(metric_buffer, sizeof(metric_buffer), "%d dBm", ap_info.rssi);
    return metric_buffer;
}

static const char* format_wifi_tx_power(void)
{
    int8_t power;
    esp_err_t ret = esp_wifi_get_max_tx_power(&power);
    
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: WiFi power unavailable");
        return metric_buffer;
    }
    
    snprintf(metric_buffer, sizeof(metric_buffer), "%d dBm", power / 4); // Convert to dBm
    return metric_buffer;
}

static const char* format_ip_address(void)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: WiFi interface not found");
        return metric_buffer;
    }
    
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(netif, &ip_info);
    
    if (ret != ESP_OK || ip_info.ip.addr == 0) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: No IP address assigned");
        return metric_buffer;
    }
    
    snprintf(metric_buffer, sizeof(metric_buffer), IPSTR, IP2STR(&ip_info.ip));
    return metric_buffer;
}

static const char* format_wifi_status(void)
{
    wifi_mode_t mode;
    esp_err_t ret = esp_wifi_get_mode(&mode);
    
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: WiFi not initialized");
        return metric_buffer;
    }
    
    wifi_ap_record_t ap_info;
    ret = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (ret == ESP_OK) {
        snprintf(metric_buffer, sizeof(metric_buffer), "Connected to %s", (char*)ap_info.ssid);
    } else {
        snprintf(metric_buffer, sizeof(metric_buffer), "Not connected");
    }
    
    return metric_buffer;
}

static const char* format_chip_id(void)
{
    uint8_t mac[6];
    esp_err_t ret = esp_efuse_mac_get_default(mac);
    
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_HARDWARE_FAULT;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Cannot read chip ID");
        return metric_buffer;
    }
    
    snprintf(metric_buffer, sizeof(metric_buffer), "%02X%02X%02X%02X%02X%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return metric_buffer;
}

static const char* format_mac_address(void)
{
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_HARDWARE_FAULT;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Cannot read MAC address");
        return metric_buffer;
    }
    
    snprintf(metric_buffer, sizeof(metric_buffer), "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return metric_buffer;
}

static const char* format_flash_size(void)
{
    uint32_t flash_size;
    esp_err_t ret = esp_flash_get_size(NULL, &flash_size);
    
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_HARDWARE_FAULT;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Cannot read flash size");
        return metric_buffer;
    }
    
    if (flash_size >= 1024 * 1024) {
        snprintf(metric_buffer, sizeof(metric_buffer), "%.1f MB", flash_size / (1024.0 * 1024.0));
    } else {
        snprintf(metric_buffer, sizeof(metric_buffer), "%lu KB", flash_size / 1024);
    }
    return metric_buffer;
}

static const char* format_chip_revision(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    snprintf(metric_buffer, sizeof(metric_buffer), "v%d.%d", 
             chip_info.revision / 100, chip_info.revision % 100);
    return metric_buffer;
}

static const char* format_core_count(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    snprintf(metric_buffer, sizeof(metric_buffer), "%d cores", chip_info.cores);
    return metric_buffer;
}

static const char* format_task_count(void)
{
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    snprintf(metric_buffer, sizeof(metric_buffer), "%d tasks", task_count);
    return metric_buffer;
}

static const char* format_task_stack_hwm(void)
{
    UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
    snprintf(metric_buffer, sizeof(metric_buffer), "%d bytes remaining", hwm * sizeof(StackType_t));
    return metric_buffer;
}

static const char* format_task_runtime_stats(void)
{
    // Note: Runtime statistics require configGENERATE_RUN_TIME_STATS=1 in FreeRTOS config
    // For simplicity, return task count instead
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    snprintf(metric_buffer, sizeof(metric_buffer), "%d tasks active", task_count);
    return metric_buffer;
}

static const char* format_task_priority(void)
{
    UBaseType_t priority = uxTaskPriorityGet(NULL);
    snprintf(metric_buffer, sizeof(metric_buffer), "%d", priority);
    return metric_buffer;
}

static const char* format_power_mode(void)
{
    esp_pm_config_t pm_config;
    esp_err_t ret = esp_pm_get_configuration(&pm_config);
    
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Power management not configured");
        return metric_buffer;
    }
    
    snprintf(metric_buffer, sizeof(metric_buffer), "Max: %d MHz, Min: %d MHz", 
             pm_config.max_freq_mhz, pm_config.min_freq_mhz);
    return metric_buffer;
}

static const char* format_vdd33_voltage(void)
{
    if (adc_handle == NULL || adc_cali_handle == NULL) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: ADC not initialized");
        return metric_buffer;
    }
    
    int adc_reading;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL_0, &adc_reading);
    
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_HARDWARE_FAULT;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: ADC read failed");
        return metric_buffer;
    }
    
    int voltage_mv;
    ret = adc_cali_raw_to_voltage(adc_cali_handle, adc_reading, &voltage_mv);
    
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_HARDWARE_FAULT;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: ADC calibration failed");
        return metric_buffer;
    }
    
    float voltage_v = voltage_mv / 1000.0f;
    snprintf(metric_buffer, sizeof(metric_buffer), "%.2f V", voltage_v);
    return metric_buffer;
}

static const char* format_wifi_tx_rx_bytes(void)
{
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: WiFi not connected");
        return metric_buffer;
    }
    
    // Note: ESP-IDF doesn't provide direct TX/RX byte counters
    // This would require custom implementation using packet callbacks
    snprintf(metric_buffer, sizeof(metric_buffer), "Feature not implemented");
    return metric_buffer;
}

static const char* format_network_speed(void)
{
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: WiFi not connected");
        return metric_buffer;
    }
    
    // Estimate speed based on PHY mode
    const char* phy_mode;
    uint32_t max_speed_mbps = 0;
    
    switch (ap_info.phy_11b) {
        case 1: phy_mode = "802.11b"; max_speed_mbps = 11; break;
        default:
            switch (ap_info.phy_11g) {
                case 1: phy_mode = "802.11g"; max_speed_mbps = 54; break;
                default:
                    switch (ap_info.phy_11n) {
                        case 1: phy_mode = "802.11n"; max_speed_mbps = 150; break;
                        default: phy_mode = "Unknown"; max_speed_mbps = 0; break;
                    }
                    break;
            }
            break;
    }
    
    snprintf(metric_buffer, sizeof(metric_buffer), "%s (up to %lu Mbps)", phy_mode, max_speed_mbps);
    return metric_buffer;
}

static const char* format_flash_usage(void)
{
    const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
    
    if (partition == NULL) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: No data partition found");
        return metric_buffer;
    }
    
    size_t total_size = partition->size;
    // Note: Used space calculation would require filesystem-specific APIs
    snprintf(metric_buffer, sizeof(metric_buffer), "Total: %zu bytes", total_size);
    return metric_buffer;
}

static const char* format_spiffs_usage(void)
{
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(NULL, &total, &used);
    
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: SPIFFS not mounted");
        return metric_buffer;
    }
    
    float usage_percent = (float)used / total * 100.0f;
    snprintf(metric_buffer, sizeof(metric_buffer), "%zu/%zu bytes (%.1f%%)", used, total, usage_percent);
    return metric_buffer;
}

static const char* format_boot_count(void)
{
    if (metrics_nvs_handle == 0) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: NVS not available");
        return metric_buffer;
    }
    
    uint32_t boot_count = 0;
    size_t required_size = sizeof(boot_count);
    esp_err_t ret = nvs_get_blob(metrics_nvs_handle, "boot_count", &boot_count, &required_size);
    
    if (ret != ESP_OK) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Boot count not available");
        return metric_buffer;
    }
    
    snprintf(metric_buffer, sizeof(metric_buffer), "%lu boots", boot_count);
    return metric_buffer;
}

static const char* format_crash_count(void)
{
    if (metrics_nvs_handle == 0) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: NVS not available");
        return metric_buffer;
    }
    
    uint32_t crash_count = 0;
    size_t required_size = sizeof(crash_count);
    esp_err_t ret = nvs_get_blob(metrics_nvs_handle, "crash_count", &crash_count, &required_size);
    
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        // First time - check if last reset was due to crash
        esp_reset_reason_t reason = esp_reset_reason();
        if (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT) {
            crash_count = 1;
            nvs_set_blob(metrics_nvs_handle, "crash_count", &crash_count, sizeof(crash_count));
            nvs_commit(metrics_nvs_handle);
        }
    } else if (ret != ESP_OK) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Crash count not available");
        return metric_buffer;
    }
    
    snprintf(metric_buffer, sizeof(metric_buffer), "%lu crashes", crash_count);
    return metric_buffer;
}

static const char* format_light_sleep_duration(void)
{
    // Note: ESP-IDF doesn't provide direct sleep time tracking
    // This would require custom implementation with sleep/wake callbacks
    last_error = METRIC_ERROR_NOT_AVAILABLE;
    snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Light sleep duration tracking not implemented");
    return metric_buffer;
}

static const char* format_deep_sleep_duration(void)
{
    // Note: Deep sleep duration could be tracked in RTC memory
    // For now, return not available
    last_error = METRIC_ERROR_NOT_AVAILABLE;
    snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Deep sleep duration tracking not implemented");
    return metric_buffer;
}

static const char* format_current_consumption(void)
{
    // Note: Current consumption measurement requires external hardware
    // This is typically measured with a current sense resistor and ADC
    last_error = METRIC_ERROR_NOT_SUPPORTED;
    snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Current measurement requires external hardware");
    return metric_buffer;
}

static const char* format_bt_ble_rssi(void)
{
#ifdef CONFIG_BT_ENABLED
    // Note: BT/BLE RSSI requires active connection
    last_error = METRIC_ERROR_NOT_AVAILABLE;
    snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: BT/BLE not connected");
    return metric_buffer;
#else
    last_error = METRIC_ERROR_NOT_SUPPORTED;
    snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Bluetooth not enabled in configuration");
    return metric_buffer;
#endif
}

static const char* format_bt_ble_connected_devices(void)
{
#ifdef CONFIG_BT_ENABLED
    // Note: Would require BT/BLE stack initialization and connection tracking
    last_error = METRIC_ERROR_NOT_AVAILABLE;
    snprintf(metric_buffer, sizeof(metric_buffer), "0 devices");
    return metric_buffer;
#else
    last_error = METRIC_ERROR_NOT_SUPPORTED;
    snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Bluetooth not enabled in configuration");
    return metric_buffer;
#endif
}

static const char* format_flash_rw_operations(void)
{
    // Note: Flash R/W operation counting requires custom implementation
    // ESP-IDF doesn't provide built-in counters for this
    last_error = METRIC_ERROR_NOT_AVAILABLE;
    snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: Flash R/W operation counting not implemented");
    return metric_buffer;
}

static const char* format_i2c_bus_errors(void)
{
    // Note: I2C error counting requires custom implementation in I2C driver
    last_error = METRIC_ERROR_NOT_AVAILABLE;
    snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: I2C error counting not implemented");
    return metric_buffer;
}

static const char* format_spi_performance(void)
{
    // Note: SPI performance metrics require custom implementation
    last_error = METRIC_ERROR_NOT_AVAILABLE;
    snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: SPI performance monitoring not implemented");
    return metric_buffer;
}

static const char* format_gpio_status(void)
{
    // Note: GPIO status could be implemented by reading all pin states
    // For now, return a simple placeholder
    last_error = METRIC_ERROR_NOT_AVAILABLE;
    snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: GPIO status monitoring not implemented");
    return metric_buffer;
}

static const char* format_ota_update_status(void)
{
    // Check if an OTA update is available or in progress
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* boot = esp_ota_get_boot_partition();
    
    if (!running || !boot) {
        // OTA partitions not found or not properly initialized
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: OTA partition info not available");
        return metric_buffer;
    }
    
    if (running != boot) {
        snprintf(metric_buffer, sizeof(metric_buffer), "Update pending (boot from %s)", boot->label);
    } else {
        snprintf(metric_buffer, sizeof(metric_buffer), "Up to date (running %s)", running->label);
    }
    return metric_buffer;
}

static const char* format_last_update_time(void)
{
    // Note: This would require storing update timestamps in NVS
    if (metrics_nvs_handle == 0) {
        last_error = METRIC_ERROR_NOT_AVAILABLE;
        snprintf(metric_buffer, sizeof(metric_buffer), "ERROR: NVS not available");
        return metric_buffer;
    }
    
    uint64_t last_update = 0;
    size_t required_size = sizeof(last_update);
    esp_err_t ret = nvs_get_blob(metrics_nvs_handle, "last_update", &last_update, &required_size);
    
    if (ret != ESP_OK) {
        snprintf(metric_buffer, sizeof(metric_buffer), "Never updated");
    } else {
        // Convert timestamp to readable format
        time_t timestamp = (time_t)(last_update / 1000000); // Convert microseconds to seconds
        struct tm timeinfo;
        localtime_r(&timestamp, &timeinfo);
        strftime(metric_buffer, sizeof(metric_buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    }
    return metric_buffer;
}

static const char* format_app_specific_timers(void)
{
    // Note: Application-specific timers would be defined by the application
    // This is a placeholder implementation
    uint64_t uptime_us = esp_timer_get_time();
    uint32_t uptime_seconds = uptime_us / 1000000;
    
    snprintf(metric_buffer, sizeof(metric_buffer), "App timer: %lu seconds", uptime_seconds);
    return metric_buffer;
}

bool update_boot_count(uint32_t new_count)
{
    if (metrics_nvs_handle == 0) {
        ESP_LOGE(TAG, "SystemMetrics not initialized - cannot update boot count");
        return false;
    }
    
    esp_err_t ret = nvs_set_blob(metrics_nvs_handle, "boot_count", &new_count, sizeof(new_count));
    if (ret == ESP_OK) {
        ret = nvs_commit(metrics_nvs_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Updated boot count to: %lu", new_count);
            return true;
        } else {
            ESP_LOGE(TAG, "Failed to commit boot count update: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set boot count in NVS: %s", esp_err_to_name(ret));
    }
    
    return false;
}

bool get_boot_count(uint32_t *count)
{
    if (!count) {
        ESP_LOGE(TAG, "Invalid count parameter");
        return false;
    }
    
    if (metrics_nvs_handle == 0) {
        ESP_LOGE(TAG, "SystemMetrics not initialized - cannot get boot count");
        return false;
    }
    
    size_t required_size = sizeof(*count);
    esp_err_t ret = nvs_get_blob(metrics_nvs_handle, "boot_count", count, &required_size);
    
    if (ret == ESP_OK) {
        return true;
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Boot count not found in NVS, returning 0");
        *count = 0;
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to get boot count from NVS: %s", esp_err_to_name(ret));
        return false;
    }
}