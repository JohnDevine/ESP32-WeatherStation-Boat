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
#define BRIDGE_SSID_MAX_LEN 32
#define BRIDGE_PASS_MAX_LEN 64
#define MQTT_IP_MAX_LEN 16
#define MQTT_PASS_MAX_LEN 64
#define MQTT_CLIENT_ID_MAX_LEN 32
#define MQTT_USER_MAX_LEN 32
#define MQTT_BASE_TOPIC_MAX_LEN 64

// Device role definitions
#define DEVICE_ROLE_GATEWAY 1
#define DEVICE_ROLE_RESPONDER 2

// MQTT defaults
#define MQTT_DEFAULT_PORT 1883
#define MQTT_DEFAULT_QOS 0

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

/**
 * @brief Store boot count to NVS
 *
 * @param count Boot count value
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_boot_count(uint32_t count);

/**
 * @brief Load boot count from NVS
 *
 * @param count Pointer to store boot count value
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_boot_count(uint32_t *count);

/**
 * @brief Store device role to NVS
 *
 * @param role Device role (DEVICE_ROLE_GATEWAY or DEVICE_ROLE_RESPONDER)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_device_role(uint8_t role);

/**
 * @brief Load device role from NVS
 *
 * @param role Pointer to store device role value
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_device_role(uint8_t *role);

/**
 * @brief Store bridge WiFi SSID to NVS
 *
 * @param ssid Bridge WiFi SSID string
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_bridge_ssid(const char *ssid);

/**
 * @brief Load bridge WiFi SSID from NVS
 *
 * @param ssid Buffer to store SSID string
 * @param len Length of buffer (should be BRIDGE_SSID_MAX_LEN)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_bridge_ssid(char *ssid, size_t len);

/**
 * @brief Store bridge WiFi password to NVS
 *
 * @param password Bridge WiFi password string
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_bridge_password(const char *password);

/**
 * @brief Load bridge WiFi password from NVS
 *
 * @param password Buffer to store password string
 * @param len Length of buffer (should be BRIDGE_PASS_MAX_LEN)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_bridge_password(char *password, size_t len);

/**
 * @brief Store MQTT server IP address to NVS
 *
 * @param ip MQTT server IP address string
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_mqtt_server_ip(const char *ip);

/**
 * @brief Load MQTT server IP address from NVS
 *
 * @param ip Buffer to store IP address string
 * @param len Length of buffer (should be MQTT_IP_MAX_LEN)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_mqtt_server_ip(char *ip, size_t len);

/**
 * @brief Store MQTT server port to NVS
 *
 * @param port MQTT server port
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_mqtt_port(uint16_t port);

/**
 * @brief Load MQTT server port from NVS
 *
 * @param port Pointer to store port value
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_mqtt_port(uint16_t *port);

/**
 * @brief Store MQTT username to NVS
 *
 * @param username MQTT username string
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_mqtt_username(const char *username);

/**
 * @brief Load MQTT username from NVS
 *
 * @param username Buffer to store username string
 * @param len Length of buffer (should be MQTT_USER_MAX_LEN)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_mqtt_username(char *username, size_t len);

/**
 * @brief Store MQTT password to NVS
 *
 * @param password MQTT password string
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_mqtt_password(const char *password);

/**
 * @brief Load MQTT password from NVS
 *
 * @param password Buffer to store password string
 * @param len Length of buffer (should be MQTT_PASS_MAX_LEN)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_mqtt_password(char *password, size_t len);

/**
 * @brief Store MQTT client ID to NVS
 *
 * @param client_id MQTT client ID string
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_mqtt_client_id(const char *client_id);

/**
 * @brief Load MQTT client ID from NVS
 *
 * @param client_id Buffer to store client ID string
 * @param len Length of buffer (should be MQTT_CLIENT_ID_MAX_LEN)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_mqtt_client_id(char *client_id, size_t len);

/**
 * @brief Store MQTT QoS level to NVS
 *
 * @param qos MQTT QoS level (0, 1, or 2)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_mqtt_qos(uint8_t qos);

/**
 * @brief Load MQTT QoS level from NVS
 *
 * @param qos Pointer to store QoS value
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_mqtt_qos(uint8_t *qos);

/**
 * @brief Store MQTT base topic to NVS
 *
 * @param topic MQTT base topic string
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_store_mqtt_base_topic(const char *topic);

/**
 * @brief Load MQTT base topic from NVS
 *
 * @param topic Buffer to store base topic string
 * @param len Length of buffer (should be MQTT_BASE_TOPIC_MAX_LEN)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t nvs_load_mqtt_base_topic(char *topic, size_t len);

#ifdef __cplusplus
}
#endif

#endif // NVS_UTILS_H