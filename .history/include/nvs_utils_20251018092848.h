/**
 * @file nvs_utils.h
 * @brief NVS utility functions for configuration management
 * @version 1.0.0
 * @date 2025-10-18
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

#ifndef NVS_UTILS_H
#define NVS_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================
// Constants & Definitions
// =============================
#define NVS_NAMESPACE "config"
#define MAC_ADDR_STR_LEN 18
#define IP_ADDR_STR_LEN 16
#define WIFI_PASS_MAX_LEN 64
#define ESPNOW_KEY_LEN 16

// =============================
// Function Prototypes
// =============================

/**
 * @brief Initialize NVS storage system
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_utils_init(void);

/**
 * @brief Store server MAC address to NVS
 *
 * @param mac MAC address string (format: "AA:BB:CC:DD:EE:FF")
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_server_mac(const char *mac);

/**
 * @brief Load server MAC address from NVS
 *
 * @param mac Buffer to store MAC address string
 * @param len Length of buffer (should be MAC_ADDR_STR_LEN)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_server_mac(char *mac, size_t len);

/**
 * @brief Store IP address to NVS
 *
 * @param ip IP address string (format: "192.168.1.100")
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_ip_address(const char *ip);

/**
 * @brief Load IP address from NVS
 *
 * @param ip Buffer to store IP address string
 * @param len Length of buffer (should be IP_ADDR_STR_LEN)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_ip_address(char *ip, size_t len);

/**
 * @brief Store WiFi password to NVS
 *
 * @param password WiFi password string
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_wifi_password(const char *password);

/**
 * @brief Load WiFi password from NVS
 *
 * @param password Buffer to store password string
 * @param len Length of buffer (should be WIFI_PASS_MAX_LEN)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_wifi_password(char *password, size_t len);

/**
 * @brief Store ESP-NOW active encryption key to NVS
 *
 * @param key 16-byte encryption key
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_espnow_active_key(const uint8_t *key);

/**
 * @brief Load ESP-NOW active encryption key from NVS
 *
 * @param key Buffer to store 16-byte encryption key
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_espnow_active_key(uint8_t *key);

/**
 * @brief Store ESP-NOW pending encryption key to NVS
 *
 * @param key 16-byte encryption key
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_espnow_pending_key(const uint8_t *key);

/**
 * @brief Load ESP-NOW pending encryption key from NVS
 *
 * @param key Buffer to store 16-byte encryption key
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_espnow_pending_key(uint8_t *key);

#ifdef __cplusplus
}
#endif

#endif // NVS_UTILS_H