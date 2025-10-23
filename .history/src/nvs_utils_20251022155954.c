/**
 * @file nvs_utils.c
 * @brief NVS utility functions implementation
 * @version 1.0.0
 * @date 2025-10-18
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

// =============================
// Includes
// =============================
#include "nvs_utils.h"
#include "version.h"
#include "SystemMetrics.h"
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

// =============================
// Constants & Definitions
// =============================
// Register nvs_utils.c version
REGISTER_VERSION(NvsUtils, "1.0.0", "2025-10-18");
static const char *TAG = "NVS_UTILS";

// NVS Keys (max 15 characters)
#define KEY_SERVER_MAC "server_mac"
#define KEY_IP_ADDR "ip_addr"
#define KEY_WIFI_PASS "wifi_pass"
#define KEY_ESPNOW_ACTIVE "espnow_active"
#define KEY_ESPNOW_PENDING "espnow_pending"
#define KEY_DEVICE_ROLE "device_role"
#define KEY_BRIDGE_SSID "bridge_ssid"
#define KEY_BRIDGE_PASS "bridge_pass"
#define KEY_MQTT_IP "mqtt_ip"
#define KEY_MQTT_PORT "mqtt_port"
#define KEY_MQTT_USER "mqtt_user"
#define KEY_MQTT_PASS "mqtt_pass"
#define KEY_MQTT_CLIENT "mqtt_client"
#define KEY_MQTT_QOS "mqtt_qos"
#define KEY_MQTT_TOPIC "mqtt_topic"

// =============================
// Function Definitions
// =============================

esp_err_t nvs_utils_init(void) {
    ESP_LOGI(TAG, "Initializing NVS...");
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "NVS initialized successfully");
    } else {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t nvs_store_server_mac(const char *mac) {
    if (!mac || strlen(mac) >= MAC_ADDR_STR_LEN) {
        ESP_LOGE(TAG, "Invalid MAC address parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(nvs_handle, KEY_SERVER_MAC, mac);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored server MAC: %s", mac);
        } else {
            ESP_LOGE(TAG, "Failed to commit MAC to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set MAC in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_server_mac(char *mac, size_t len) {
    if (!mac || len < MAC_ADDR_STR_LEN) {
        ESP_LOGE(TAG, "Invalid MAC buffer parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "NVS namespace not found (first boot), using default MAC");
            strcpy(mac, "00:00:00:00:00:00");
            return ESP_OK;
        }
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t required_size = len;
    err = nvs_get_str(nvs_handle, KEY_SERVER_MAC, mac, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded server MAC: %s", mac);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Server MAC not found in NVS, using default");
        strcpy(mac, "00:00:00:00:00:00");
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get MAC from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_ip_address(const char *ip) {
    if (!ip || strlen(ip) >= IP_ADDR_STR_LEN) {
        ESP_LOGE(TAG, "Invalid IP address parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(nvs_handle, KEY_IP_ADDR, ip);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored IP address: %s", ip);
        } else {
            ESP_LOGE(TAG, "Failed to commit IP to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set IP in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_ip_address(char *ip, size_t len) {
    if (!ip || len < IP_ADDR_STR_LEN) {
        ESP_LOGE(TAG, "Invalid IP buffer parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t required_size = len;
    err = nvs_get_str(nvs_handle, KEY_IP_ADDR, ip, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded IP address: %s", ip);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "IP address not found in NVS, using default");
        strcpy(ip, "192.168.1.100");
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get IP from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_wifi_password(const char *password) {
    if (!password || strlen(password) >= WIFI_PASS_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid WiFi password parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(nvs_handle, KEY_WIFI_PASS, password);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored WiFi password");
        } else {
            ESP_LOGE(TAG, "Failed to commit WiFi password to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set WiFi password in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_wifi_password(char *password, size_t len) {
    if (!password || len < WIFI_PASS_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid WiFi password buffer parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t required_size = len;
    err = nvs_get_str(nvs_handle, KEY_WIFI_PASS, password, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded WiFi password");
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "WiFi password not found in NVS, using default");
        strcpy(password, "12345678");
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get WiFi password from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_espnow_active_key(const uint8_t *key) {
    if (!key) {
        ESP_LOGE(TAG, "Invalid ESP-NOW active key parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_blob(nvs_handle, KEY_ESPNOW_ACTIVE, key, ESPNOW_KEY_LEN);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored ESP-NOW active key");
        } else {
            ESP_LOGE(TAG, "Failed to commit ESP-NOW active key to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set ESP-NOW active key in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_espnow_active_key(uint8_t *key) {
    if (!key) {
        ESP_LOGE(TAG, "Invalid ESP-NOW active key buffer parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t required_size = ESPNOW_KEY_LEN;
    err = nvs_get_blob(nvs_handle, KEY_ESPNOW_ACTIVE, key, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded ESP-NOW active key");
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "ESP-NOW active key not found in NVS, using default");
        memset(key, 0, ESPNOW_KEY_LEN);
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get ESP-NOW active key from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_espnow_pending_key(const uint8_t *key) {
    if (!key) {
        ESP_LOGE(TAG, "Invalid ESP-NOW pending key parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_blob(nvs_handle, KEY_ESPNOW_PENDING, key, ESPNOW_KEY_LEN);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored ESP-NOW pending key");
        } else {
            ESP_LOGE(TAG, "Failed to commit ESP-NOW pending key to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set ESP-NOW pending key in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_espnow_pending_key(uint8_t *key) {
    if (!key) {
        ESP_LOGE(TAG, "Invalid ESP-NOW pending key buffer parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t required_size = ESPNOW_KEY_LEN;
    err = nvs_get_blob(nvs_handle, KEY_ESPNOW_PENDING, key, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded ESP-NOW pending key");
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "ESP-NOW pending key not found in NVS, using default");
        memset(key, 0, ESPNOW_KEY_LEN);
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get ESP-NOW pending key from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_boot_count(uint32_t count) {
    // Use SystemMetrics API to update boot count
    if (update_boot_count(count)) {
        ESP_LOGI(TAG, "Boot count updated via SystemMetrics: %lu", count);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to update boot count via SystemMetrics");
        return ESP_FAIL;
    }
}

esp_err_t nvs_load_boot_count(uint32_t *count) {
    if (!count) {
        ESP_LOGE(TAG, "Invalid boot count parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Use SystemMetrics API to get boot count - single source of truth
    if (get_boot_count(count)) {
        ESP_LOGI(TAG, "Boot count loaded via SystemMetrics: %lu", *count);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to load boot count via SystemMetrics, falling back to default");
        *count = 0; // Provide default value
        return ESP_OK; // Return success with default to avoid breaking the web interface
    }
}

esp_err_t nvs_store_device_role(uint8_t role) {
    // Validate role value
    if (role != DEVICE_ROLE_GATEWAY && role != DEVICE_ROLE_RESPONDER) {
        ESP_LOGE(TAG, "Invalid device role: %d (must be %d or %d)", 
                 role, DEVICE_ROLE_GATEWAY, DEVICE_ROLE_RESPONDER);
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_u8(nvs_handle, KEY_DEVICE_ROLE, role);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored device role: %d (%s)", role, 
                     role == DEVICE_ROLE_GATEWAY ? "Gateway" : "Responder");
        } else {
            ESP_LOGE(TAG, "Failed to commit device role to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set device role in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_device_role(uint8_t *role) {
    if (!role) {
        ESP_LOGE(TAG, "Invalid device role parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "NVS namespace not found (first boot), using default device role");
            *role = DEVICE_ROLE_RESPONDER; // Default to responder
            return ESP_OK;
        }
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_get_u8(nvs_handle, KEY_DEVICE_ROLE, role);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded device role: %d (%s)", *role, 
                 *role == DEVICE_ROLE_GATEWAY ? "Gateway" : "Responder");
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Device role not found in NVS, using default (Responder)");
        *role = DEVICE_ROLE_RESPONDER; // Default to responder
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get device role from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_bridge_ssid(const char *ssid) {
    if (!ssid || strlen(ssid) >= BRIDGE_SSID_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid bridge SSID parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(nvs_handle, KEY_BRIDGE_SSID, ssid);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored bridge SSID: %s", ssid);
        } else {
            ESP_LOGE(TAG, "Failed to commit bridge SSID to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set bridge SSID in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_bridge_ssid(char *ssid, size_t len) {
    if (!ssid || len < BRIDGE_SSID_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid bridge SSID buffer parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t required_size = len;
    err = nvs_get_str(nvs_handle, KEY_BRIDGE_SSID, ssid, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded bridge SSID: %s", ssid);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Bridge SSID not found in NVS, using default");
        strcpy(ssid, "MyBridgeWiFi");
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get bridge SSID from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_bridge_password(const char *password) {
    if (!password || strlen(password) >= BRIDGE_PASS_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid bridge password parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(nvs_handle, KEY_BRIDGE_PASS, password);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored bridge password");
        } else {
            ESP_LOGE(TAG, "Failed to commit bridge password to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set bridge password in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_bridge_password(char *password, size_t len) {
    if (!password || len < BRIDGE_PASS_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid bridge password buffer parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t required_size = len;
    err = nvs_get_str(nvs_handle, KEY_BRIDGE_PASS, password, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded bridge password");
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Bridge password not found in NVS, using default");
        strcpy(password, "bridgepass123");
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get bridge password from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_mqtt_server_ip(const char *ip) {
    if (!ip || strlen(ip) >= MQTT_IP_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid MQTT server IP parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(nvs_handle, KEY_MQTT_IP, ip);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored MQTT server IP: %s", ip);
        } else {
            ESP_LOGE(TAG, "Failed to commit MQTT server IP to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set MQTT server IP in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_mqtt_server_ip(char *ip, size_t len) {
    if (!ip || len < MQTT_IP_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid MQTT server IP buffer parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t required_size = len;
    err = nvs_get_str(nvs_handle, KEY_MQTT_IP, ip, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded MQTT server IP: %s", ip);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "MQTT server IP not found in NVS, using default");
        strcpy(ip, "192.168.1.200");
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get MQTT server IP from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_mqtt_port(uint16_t port) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_u16(nvs_handle, KEY_MQTT_PORT, port);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored MQTT port: %d", port);
        } else {
            ESP_LOGE(TAG, "Failed to commit MQTT port to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set MQTT port in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_mqtt_port(uint16_t *port) {
    if (!port) {
        ESP_LOGE(TAG, "Invalid MQTT port parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_get_u16(nvs_handle, KEY_MQTT_PORT, port);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded MQTT port: %d", *port);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "MQTT port not found in NVS, using default");
        *port = MQTT_DEFAULT_PORT;
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get MQTT port from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_mqtt_username(const char *username) {
    if (!username || strlen(username) >= MQTT_USER_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid MQTT username parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(nvs_handle, KEY_MQTT_USER, username);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored MQTT username: %s", username);
        } else {
            ESP_LOGE(TAG, "Failed to commit MQTT username to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set MQTT username in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_mqtt_username(char *username, size_t len) {
    if (!username || len < MQTT_USER_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid MQTT username buffer parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t required_size = len;
    err = nvs_get_str(nvs_handle, KEY_MQTT_USER, username, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded MQTT username: %s", username);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "MQTT username not found in NVS, using default");
        strcpy(username, "mqttuser");
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get MQTT username from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_mqtt_password(const char *password) {
    if (!password || strlen(password) >= MQTT_PASS_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid MQTT password parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(nvs_handle, KEY_MQTT_PASS, password);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored MQTT password");
        } else {
            ESP_LOGE(TAG, "Failed to commit MQTT password to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set MQTT password in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_mqtt_password(char *password, size_t len) {
    if (!password || len < MQTT_PASS_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid MQTT password buffer parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t required_size = len;
    err = nvs_get_str(nvs_handle, KEY_MQTT_PASS, password, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded MQTT password");
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "MQTT password not found in NVS, using default");
        strcpy(password, "mqttpass123");
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get MQTT password from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_mqtt_client_id(const char *client_id) {
    if (!client_id || strlen(client_id) >= MQTT_CLIENT_ID_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid MQTT client ID parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(nvs_handle, KEY_MQTT_CLIENT, client_id);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored MQTT client ID: %s", client_id);
        } else {
            ESP_LOGE(TAG, "Failed to commit MQTT client ID to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set MQTT client ID in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_mqtt_client_id(char *client_id, size_t len) {
    if (!client_id || len < MQTT_CLIENT_ID_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid MQTT client ID buffer parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t required_size = len;
    err = nvs_get_str(nvs_handle, KEY_MQTT_CLIENT, client_id, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded MQTT client ID: %s", client_id);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "MQTT client ID not found in NVS, using default");
        strcpy(client_id, "ESP32WeatherStation");
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get MQTT client ID from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_mqtt_qos(uint8_t qos) {
    // Validate QoS value (0, 1, or 2)
    if (qos > 2) {
        ESP_LOGE(TAG, "Invalid MQTT QoS value: %d (must be 0, 1, or 2)", qos);
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_u8(nvs_handle, KEY_MQTT_QOS, qos);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored MQTT QoS: %d", qos);
        } else {
            ESP_LOGE(TAG, "Failed to commit MQTT QoS to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set MQTT QoS in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_mqtt_qos(uint8_t *qos) {
    if (!qos) {
        ESP_LOGE(TAG, "Invalid MQTT QoS parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_get_u8(nvs_handle, KEY_MQTT_QOS, qos);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded MQTT QoS: %d", *qos);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "MQTT QoS not found in NVS, using default");
        *qos = MQTT_DEFAULT_QOS;
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get MQTT QoS from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_store_mqtt_base_topic(const char *topic) {
    if (!topic || strlen(topic) >= MQTT_BASE_TOPIC_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid MQTT base topic parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(nvs_handle, KEY_MQTT_TOPIC, topic);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Stored MQTT base topic: %s", topic);
        } else {
            ESP_LOGE(TAG, "Failed to commit MQTT base topic to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set MQTT base topic in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_mqtt_base_topic(char *topic, size_t len) {
    if (!topic || len < MQTT_BASE_TOPIC_MAX_LEN) {
        ESP_LOGE(TAG, "Invalid MQTT base topic buffer parameter");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    size_t required_size = len;
    err = nvs_get_str(nvs_handle, KEY_MQTT_TOPIC, topic, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded MQTT base topic: %s", topic);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "MQTT base topic not found in NVS, using default");
        strcpy(topic, "weatherstation");
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get MQTT base topic from NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}