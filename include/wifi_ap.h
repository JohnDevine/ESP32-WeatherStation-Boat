/**
 * @file wifi_ap.h
 * @brief WiFi Access Point management functions
 * @version 1.0.0
 * @date 2025-10-18
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

#ifndef WIFI_AP_H
#define WIFI_AP_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================
// Constants & Definitions
// =============================
#define AP_SSID "ESP32WebServer"
#define AP_DEFAULT_PASSWORD "12345678"
#define AP_CHANNEL 1
#define AP_MAX_CONNECTIONS 4

// =============================
// Function Prototypes
// =============================

/**
 * @brief Initialize WiFi Access Point
 *
 * Sets up ESP32 as an Access Point with configurable password from NVS
 * IP: 192.168.4.1, Gateway: 192.168.4.1, Netmask: 255.255.255.0
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t wifi_ap_init(void);

/**
 * @brief Get current AP status
 *
 * @return true if AP is running, false otherwise
 */
bool wifi_ap_is_running(void);

/**
 * @brief Stop WiFi Access Point
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t wifi_ap_stop(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_AP_H