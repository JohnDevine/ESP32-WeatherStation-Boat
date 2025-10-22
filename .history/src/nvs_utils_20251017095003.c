/**
 * @file nvs_utils.c
 * @brief NVS utility functions implementation
 * @version 0.1
 * @date 2025-10-17
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

// =============================
// Includes
// =============================
#include "nvs_utils.h"
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

// =============================
// Constants & Definitions
// =============================
static const char *TAG = "NVS_UTILS";

// NVS Keys (max 15 characters)
#define KEY_SERVER_MAC "server_mac"
#define KEY_IP_ADDR "ip_addr"
#define KEY_WIFI_PASS "wifi_pass"
#define KEY_ESPNOW_ACTIVE "espnow_active"
#define KEY_ESPNOW_PENDING "espnow_pending"

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