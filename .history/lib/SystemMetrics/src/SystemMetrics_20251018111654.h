/**
 * @file SystemMetrics.h
 * @brief ESP32 System Metrics Library
 * 
 * This library provides access to various ESP32 system metrics including
 * performance counters, resource usage, connectivity status, and hardware
 * information. Metrics are returned as null-terminated strings.
 * 
 * @version 1.0.0
 * @date 2025-10-17
 * @author John Devine
 * @email john.h.devine@gmail.com
 * @copyright Copyright (c) 2025 John Devine
 */

#ifndef SYSTEM_METRICS_H
#define SYSTEM_METRICS_H

// =============================
// Includes
// =============================
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================
// Constants & Definitions
// =============================

/**
 * @brief Maximum length for metric string values
 */
#define METRIC_MAX_STRING_LENGTH 128

/**
 * @brief System metric identifiers
 * 
 * Enumeration of all available system metrics that can be queried.
 * Each metric returns a formatted string representation.
 */
typedef enum {
    // System Performance & Resource Metrics
    METRIC_CPU_FREQUENCY,           ///< CPU clock frequency in MHz
    METRIC_CPU_TEMPERATURE,         ///< Internal temperature sensor in Celsius
    METRIC_FREE_HEAP,              ///< Available heap memory in bytes
    METRIC_MIN_FREE_HEAP,          ///< Minimum free heap size since boot
    METRIC_UPTIME,                 ///< Time since boot in milliseconds
    METRIC_RESET_REASON,           ///< Cause of last reset/reboot
    METRIC_TASK_RUNTIME_STATS,     ///< CPU time used by current task
    METRIC_TASK_PRIORITY,          ///< Priority level of current task
    
    // Power Management Metrics
    METRIC_POWER_MODE,             ///< Current power saving mode
    METRIC_LIGHT_SLEEP_DURATION,   ///< Time spent in light sleep mode
    METRIC_DEEP_SLEEP_DURATION,    ///< Time spent in deep sleep mode
    METRIC_VDD33_VOLTAGE,          ///< Supply voltage level (3.3V rail)
    METRIC_CURRENT_CONSUMPTION,    ///< Real-time current draw (external sensor)
    
    // Connectivity Metrics (WiFi)
    METRIC_WIFI_RSSI,              ///< WiFi signal strength in dBm
    METRIC_WIFI_TX_POWER,          ///< WiFi transmission power level
    METRIC_WIFI_TX_RX_BYTES,       ///< Data transmitted/received over WiFi
    METRIC_IP_ADDRESS,             ///< Current IP address
    METRIC_WIFI_STATUS,            ///< WiFi connection status
    METRIC_NETWORK_SPEED,          ///< Current network connection speed
    
    // Connectivity Metrics (Bluetooth)
    METRIC_BT_BLE_RSSI,            ///< Bluetooth/BLE signal strength
    METRIC_BT_BLE_CONNECTED_DEVICES, ///< Number of connected BT/BLE devices
    
    // Storage & Peripheral Metrics
    METRIC_FLASH_USAGE,            ///< Used/available flash storage space
    METRIC_FLASH_RW_OPERATIONS,    ///< Count of flash read/write operations
    METRIC_SPIFFS_USAGE,           ///< SPIFFS filesystem space utilization
    METRIC_I2C_BUS_ERRORS,         ///< Error counts for I2C communications
    METRIC_SPI_PERFORMANCE,        ///< SPI transaction speed/throughput
    METRIC_GPIO_STATUS,            ///< Current state of GPIO pins
    
    // Hardware Information
    METRIC_CHIP_ID,                ///< Unique ESP32 chip identifier
    METRIC_MAC_ADDRESS,            ///< Factory-assigned MAC address
    METRIC_FLASH_SIZE,             ///< Flash memory size in bytes
    METRIC_CHIP_REVISION,          ///< Silicon revision of the chip
    METRIC_CORE_COUNT,             ///< Number of available CPU cores
    
    // Task and Runtime Information
    METRIC_TASK_COUNT,             ///< Number of active FreeRTOS tasks
    METRIC_TASK_STACK_HWM,         ///< Current task stack high water mark
    
    // Application-Specific Metrics
    METRIC_BOOT_COUNT,             ///< Number of times device has booted
    METRIC_CRASH_COUNT,            ///< Number of unexpected resets
    METRIC_OTA_UPDATE_STATUS,      ///< Status of over-the-air firmware updates
    METRIC_LAST_UPDATE_TIME,       ///< Timestamp of last firmware update
    METRIC_APP_SPECIFIC_TIMERS,    ///< Custom timing metrics for application logic
    
    METRIC_COUNT                   ///< Total number of available metrics
} system_metric_t;

/**
 * @brief Error codes for system metrics
 */
typedef enum {
    METRIC_OK = 0,                 ///< Success
    METRIC_ERROR_INVALID_ID,       ///< Invalid metric ID
    METRIC_ERROR_NOT_AVAILABLE,    ///< Metric not available (e.g., WiFi not connected)
    METRIC_ERROR_NOT_SUPPORTED,    ///< Metric not supported by this hardware
    METRIC_ERROR_HARDWARE_FAULT,   ///< Hardware sensor fault
    METRIC_ERROR_BUFFER_TOO_SMALL  ///< Output buffer too small
} metric_error_t;

// =============================
// Function Prototypes
// =============================

/**
 * @brief Initialize the system metrics library
 * 
 * This function should be called once during system startup to initialize
 * any required sensors or subsystems.
 * 
 * @return true if initialization successful, false otherwise
 */
bool system_metrics_init(void);

/**
 * @brief Get a system metric as a null-terminated string
 * 
 * Retrieves the specified system metric and formats it as a human-readable
 * string. The caller should check for error conditions when metrics are
 * not available (e.g., WiFi not connected).
 * 
 * @param metric The metric to retrieve
 * @return Pointer to null-terminated string containing the metric value,
 *         or error message if metric is unavailable. The returned pointer
 *         points to internal static storage and should not be freed.
 * 
 * @note The returned string is valid until the next call to this function.
 *       Copy the string if you need to retain it across multiple calls.
 * 
 * @example
 * ```c
 * const char* cpu_freq = get_system_metric(METRIC_CPU_FREQUENCY);
 * ESP_LOGI(TAG, "CPU Frequency: %s", cpu_freq);
 * ```
 */
const char* get_system_metric(system_metric_t metric);

/**
 * @brief Get the last error code from metric retrieval
 * 
 * @return Last error code from get_system_metric()
 */
metric_error_t get_metric_error(void);

/**
 * @brief Get a human-readable description of a metric
 * 
 * @param metric The metric to describe
 * @return Pointer to null-terminated description string
 */
const char* get_metric_description(system_metric_t metric);

/**
 * @brief Update the boot count value in NVS
 * 
 * This function allows external code to modify the boot count stored in NVS.
 * Useful for resetting or adjusting the boot count for testing purposes.
 * 
 * @param new_count The new boot count value to store
 * @return true if successful, false if failed
 */
bool update_boot_count(uint32_t new_count);

/**
 * @brief Get the raw boot count value as a number
 * 
 * This function returns the boot count as a uint32_t value instead of
 * a formatted string, which is useful for configuration interfaces.
 * 
 * @param count Pointer to store the boot count value
 * @return true if successful, false if failed
 */
bool get_boot_count(uint32_t *count);

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_METRICS_H