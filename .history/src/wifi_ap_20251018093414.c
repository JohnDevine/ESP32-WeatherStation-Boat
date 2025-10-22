/**
 * @file wifi_ap.c
 * @brief WiFi Access Point management implementation
 * @version 1.0.0
 * @date 2025-10-18
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

// =============================
// Includes
// =============================
#include "wifi_ap.h"
#include "nvs_utils.h"
#include "version.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"

// =============================
// Constants & Definitions
// =============================
// Register wifi_ap.c version
REGISTER_VERSION(WifiAp, "1.0.0", "2025-10-18");
static const char *TAG = "WIFI_AP";
static bool ap_running = false;

// =============================
// Function Definitions
// =============================

esp_err_t wifi_ap_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi Access Point...");

    // Initialize the underlying TCP/IP stack
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create default event loop if not already created
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create default WiFi AP
    esp_netif_create_default_wifi_ap();

    // Initialize WiFi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    // Load WiFi password from NVS
    char wifi_password[WIFI_PASS_MAX_LEN];
    ret = nvs_load_wifi_password(wifi_password, sizeof(wifi_password));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load WiFi password from NVS, using default");
        strcpy(wifi_password, AP_DEFAULT_PASSWORD);
    }

    // Configure AP settings
    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = AP_CHANNEL,
            .password = "",
            .max_connection = AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };

    // Copy password to config (ensure null termination)
    strncpy((char *)ap_config.ap.password, wifi_password, sizeof(ap_config.ap.password) - 1);
    ap_config.ap.password[sizeof(ap_config.ap.password) - 1] = '\0';

    // If password is empty, use open authentication
    if (strlen(wifi_password) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Set WiFi mode to Access Point
    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure the Access Point
    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure WiFi AP: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start WiFi
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    ap_running = true;
    ESP_LOGI(TAG, "WiFi Access Point started successfully");
    ESP_LOGI(TAG, "SSID: %s", AP_SSID);
    ESP_LOGI(TAG, "Channel: %d", AP_CHANNEL);
    ESP_LOGI(TAG, "Max connections: %d", AP_MAX_CONNECTIONS);
    ESP_LOGI(TAG, "IP: 192.168.4.1");

    return ESP_OK;
}

bool wifi_ap_is_running(void) {
    return ap_running;
}

esp_err_t wifi_ap_stop(void) {
    if (!ap_running) {
        ESP_LOGW(TAG, "WiFi AP is not running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping WiFi Access Point...");

    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    ap_running = false;
    ESP_LOGI(TAG, "WiFi Access Point stopped successfully");

    return ESP_OK;
}