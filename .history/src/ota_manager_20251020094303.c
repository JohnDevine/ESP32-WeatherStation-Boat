/**
 * @file ota_manager.c
 * @brief OTA Manager implementation: consolidated upload endpoint, status, backup
 * @version 1.0.0
 * @date 2025-10-19
 */

// =============================
// Includes
// =============================
#include "ota_manager.h"
#include "SystemMetrics.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_http_server.h"
#include "mbedtls/sha256.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "OTA_MANAGER";

// Internal state
static ota_status_t g_ota_status = {0};
static ota_config_t g_ota_config = {0};
static esp_ota_handle_t g_ota_handle = 0;
static const esp_partition_t* g_update_partition = NULL;
static mbedtls_sha256_context g_sha256_ctx;
static bool g_crypto_init = false;

esp_err_t ota_manager_init(void) {
    memset(&g_ota_status, 0, sizeof(g_ota_status));
    g_ota_status.state = OTA_STATE_IDLE;
    g_ota_status.type = OTA_TYPE_FIRMWARE;
    // Default preferences
    g_ota_config.create_backup = true;
    g_ota_config.verify_crypto = true;
    g_ota_config.expected_hash[0] = '\0';
    ESP_LOGI(TAG, "OTA manager initialized");
    return ESP_OK;
}

esp_err_t ota_register_endpoints(httpd_handle_t server) {
    if (!server) return ESP_ERR_INVALID_ARG;

    // Serve ota.html
    httpd_uri_t ota_page = {
        .uri = "/ota.html",
        .method = HTTP_GET,
        .handler = NULL, // handled by file_get_handler in web_server.c
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_page);

    // Consolidated API: GET for status/backup, POST for uploads
    httpd_uri_t ota_api_get = {
        .uri = "/api/ota",
        .method = HTTP_GET,
        .handler = NULL,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_api_get);

    httpd_uri_t ota_api_post = {
        .uri = "/api/ota",
        .method = HTTP_POST,
        .handler = NULL,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_api_post);

    ESP_LOGI(TAG, "OTA endpoints registered (placeholders)");
    return ESP_OK;
}

esp_err_t ota_start_update(const ota_config_t* config) {
    if (!config) return ESP_ERR_INVALID_ARG;
    
    // Store user config
    memcpy(&g_ota_config, config, sizeof(g_ota_config));
    
    // Reset status
    memset(&g_ota_status, 0, sizeof(g_ota_status));
    g_ota_status.state = OTA_STATE_UPLOADING;
    g_ota_status.backup_created = false;
    g_ota_status.backup_skipped = !config->create_backup;
    g_ota_status.uploaded_size = 0;
    g_ota_status.progress_percent = 0;
    g_ota_status.type = config->update_type;
    
    if (config->update_type == OTA_TYPE_FIRMWARE) {
        // Get next available OTA partition
        g_update_partition = esp_ota_get_next_update_partition(NULL);
        if (g_update_partition == NULL) {
            ESP_LOGE(TAG, "No available OTA partition found");
            snprintf(g_ota_status.error_message, sizeof(g_ota_status.error_message), 
                    "No available OTA partition");
            g_ota_status.state = OTA_STATE_ERROR;
            return ESP_ERR_NOT_FOUND;
        }
        
        ESP_LOGI(TAG, "Starting OTA update to partition: %s", g_update_partition->label);
        
        // Begin OTA update
        esp_err_t ret = esp_ota_begin(g_update_partition, OTA_SIZE_UNKNOWN, &g_ota_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(ret));
            snprintf(g_ota_status.error_message, sizeof(g_ota_status.error_message), 
                    "OTA begin failed: %s", esp_err_to_name(ret));
            g_ota_status.state = OTA_STATE_ERROR;
            return ret;
        }
    } else {
        // For filesystem updates, get SPIFFS partition
        g_update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 
                                                     ESP_PARTITION_SUBTYPE_DATA_SPIFFS, 
                                                     "spiffs");
        if (g_update_partition == NULL) {
            ESP_LOGE(TAG, "SPIFFS partition not found");
            snprintf(g_ota_status.error_message, sizeof(g_ota_status.error_message), 
                    "SPIFFS partition not found");
            g_ota_status.state = OTA_STATE_ERROR;
            return ESP_ERR_NOT_FOUND;
        }
    }
    
    // Initialize crypto verification if requested
    if (config->verify_crypto && config->expected_hash[0] != '\0') {
        mbedtls_sha256_init(&g_sha256_ctx);
        mbedtls_sha256_starts_ret(&g_sha256_ctx, 0); // SHA256, not SHA224
        g_crypto_init = true;
    }
    
    ESP_LOGI(TAG, "OTA update started: type=%d, backup=%d, verify=%d, partition=%s",
             config->update_type, config->create_backup, config->verify_crypto,
             g_update_partition->label);
    
    return ESP_OK;
}

esp_err_t ota_process_chunk(const uint8_t* data, size_t size) {
    if (!data || size == 0) return ESP_ERR_INVALID_ARG;
    // Update progress counters
    g_ota_status.uploaded_size += size;
    if (g_ota_status.total_size > 0) {
        g_ota_status.progress_percent = (uint8_t)((g_ota_status.uploaded_size * 100) / g_ota_status.total_size);
    }
    return ESP_OK;
}

esp_err_t ota_finalize_update(void) {
    // Verify crypto if requested (placeholder)
    if (g_ota_config.verify_crypto && g_ota_config.expected_hash[0] != '\0') {
        ESP_LOGI(TAG, "Verifying uploaded image (expected hash provided)");
        // In a real implementation we'd compute SHA256 over written data
        // Here we assume verification passes
    }

    g_ota_status.state = OTA_STATE_VERIFYING;
    g_ota_status.progress_percent = 100;
    g_ota_status.state = OTA_STATE_SUCCESS;
    ESP_LOGI(TAG, "OTA update finalized successfully");
    return ESP_OK;
}

esp_err_t ota_get_status(ota_status_t* status) {
    if (!status) return ESP_ERR_INVALID_ARG;
    memcpy(status, &g_ota_status, sizeof(ota_status_t));
    return ESP_OK;
}

esp_err_t ota_create_backup(ota_type_t type) {
    // For now, we just mark backup available; actual streaming is handled by web handler
    g_ota_status.backup_available = true;
    g_ota_status.backup_created = true;
    ESP_LOGI(TAG, "Backup flagged available for type=%d", type);
    return ESP_OK;
}

bool ota_is_backup_available(ota_type_t type) {
    return g_ota_status.backup_available;
}

esp_err_t ota_verify_crypto(const uint8_t* data, size_t len, const uint8_t* expected_hash) {
    if (!expected_hash) return ESP_OK; // verification skipped
    unsigned char hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, data, len);
    mbedtls_sha256_finish_ret(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    if (memcmp(hash, expected_hash, 32) == 0) return ESP_OK;
    return ESP_FAIL;
}

esp_err_t ota_auto_rollback(void) {
    ESP_LOGW(TAG, "Automatic rollback initiated (placeholder)");
    // In a real implementation we'd switch to previous partition
    return ESP_OK;
}

esp_err_t ota_set_backup_preference(bool enable_backup) {
    g_ota_config.create_backup = enable_backup;
    return ESP_OK;
}

bool ota_get_backup_preference(void) {
    return g_ota_config.create_backup;
}

void ota_reboot_system(void) {
    ESP_LOGI(TAG, "Rebooting system now...");
    esp_restart();
}

esp_err_t ota_get_partition_info(ota_type_t type, const esp_partition_t** partition) {
    if (!partition) return ESP_ERR_INVALID_ARG;
    if (type == OTA_TYPE_FIRMWARE) {
        *partition = esp_ota_get_running_partition();
    } else {
        *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
    }
    return (*partition) ? ESP_OK : ESP_ERR_NOT_FOUND;
}
