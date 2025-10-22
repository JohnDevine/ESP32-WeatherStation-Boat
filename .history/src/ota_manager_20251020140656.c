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
#include "esp_spiffs.h"
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
        
        ESP_LOGI(TAG, "Found SPIFFS partition: %s, size: %d bytes", 
                 g_update_partition->label, g_update_partition->size);
        
        // For SPIFFS updates, we need to erase the partition first
        ESP_LOGI(TAG, "Erasing SPIFFS partition for update...");
        ret = esp_partition_erase_range(g_update_partition, 0, g_update_partition->size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase SPIFFS partition: %s", esp_err_to_name(ret));
            snprintf(g_ota_status.error_message, sizeof(g_ota_status.error_message), 
                    "SPIFFS erase failed: %s", esp_err_to_name(ret));
            g_ota_status.state = OTA_STATE_ERROR;
            return ret;
        }
        ESP_LOGI(TAG, "SPIFFS partition erased successfully");
    }
    
    // Initialize crypto verification if requested
    if (config->verify_crypto && config->expected_hash[0] != '\0') {
        mbedtls_sha256_init(&g_sha256_ctx);
        int ret = mbedtls_sha256_starts(&g_sha256_ctx, 0); // SHA256, not SHA224
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to initialize SHA256 context");
            return ESP_FAIL;
        }
        g_crypto_init = true;
    }
    
    ESP_LOGI(TAG, "OTA update started: type=%d, backup=%d, verify=%d, partition=%s",
             config->update_type, config->create_backup, config->verify_crypto,
             g_update_partition->label);
    
    return ESP_OK;
}

esp_err_t ota_process_chunk(const uint8_t* data, size_t size) {
    if (!data || size == 0) return ESP_ERR_INVALID_ARG;
    
    esp_err_t ret = ESP_OK;
    
    // Log first few bytes to help debug
    if (g_ota_status.uploaded_size == 0 && size >= 8) {
        ESP_LOGI(TAG, "First chunk: %02x %02x %02x %02x %02x %02x %02x %02x", 
                data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
    }
    
    if (g_ota_status.type == OTA_TYPE_FIRMWARE) {
        // Write to OTA partition
        ret = esp_ota_write(g_ota_handle, data, size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(ret));
            snprintf(g_ota_status.error_message, sizeof(g_ota_status.error_message), 
                    "OTA write failed: %s", esp_err_to_name(ret));
            g_ota_status.state = OTA_STATE_ERROR;
            return ret;
        }
    } else {
        // For filesystem, write directly to partition
        size_t offset = g_ota_status.uploaded_size;
        ret = esp_partition_write(g_update_partition, offset, data, size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_partition_write failed: %s", esp_err_to_name(ret));
            snprintf(g_ota_status.error_message, sizeof(g_ota_status.error_message), 
                    "Partition write failed: %s", esp_err_to_name(ret));
            g_ota_status.state = OTA_STATE_ERROR;
            return ret;
        }
    }
    
    // Update crypto hash if verification is enabled
    if (g_crypto_init) {
        int ret = mbedtls_sha256_update(&g_sha256_ctx, data, size);
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to update SHA256 hash");
            return ESP_FAIL;
        }
    }
    
    // Update progress counters
    g_ota_status.uploaded_size += size;
    if (g_ota_status.total_size > 0) {
        g_ota_status.progress_percent = (uint8_t)((g_ota_status.uploaded_size * 100) / g_ota_status.total_size);
    }
    
    ESP_LOGD(TAG, "Processed chunk: %d bytes, total: %d bytes", size, g_ota_status.uploaded_size);
    
    return ESP_OK;
}

esp_err_t ota_finalize_update(void) {
    g_ota_status.state = OTA_STATE_VERIFYING;
    g_ota_status.progress_percent = 90;
    
    esp_err_t ret = ESP_OK;
    
    // Perform crypto verification if requested
    if (g_crypto_init && g_ota_config.expected_hash[0] != '\0') {
        ESP_LOGI(TAG, "Verifying uploaded image hash...");
        
        unsigned char computed_hash[32];
        int ret = mbedtls_sha256_finish(&g_sha256_ctx, computed_hash);
        mbedtls_sha256_free(&g_sha256_ctx);
        g_crypto_init = false;
        
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to finalize SHA256 hash");
            snprintf(g_ota_status.error_message, sizeof(g_ota_status.error_message), 
                    "Hash computation failed");
            g_ota_status.state = OTA_STATE_ERROR;
            ota_auto_rollback();
            return ESP_FAIL;
        }
        
        // Convert expected hash from hex string to binary
        unsigned char expected_hash[32];
        if (strlen(g_ota_config.expected_hash) == 64) {
            for (int i = 0; i < 32; i++) {
                char byte_str[3] = { g_ota_config.expected_hash[i*2], g_ota_config.expected_hash[i*2+1], '\0' };
                expected_hash[i] = (unsigned char)strtoul(byte_str, NULL, 16);
            }
            
            // Compare hashes
            if (memcmp(computed_hash, expected_hash, 32) != 0) {
                ESP_LOGE(TAG, "Hash verification failed!");
                snprintf(g_ota_status.error_message, sizeof(g_ota_status.error_message), 
                        "Hash verification failed");
                g_ota_status.state = OTA_STATE_ERROR;
                
                // Perform rollback
                ota_auto_rollback();
                return ESP_ERR_INVALID_CRC;
            }
            ESP_LOGI(TAG, "Hash verification successful");
        } else {
            ESP_LOGW(TAG, "Invalid hash format, skipping verification");
        }
    }
    
    g_ota_status.state = OTA_STATE_FLASHING;
    g_ota_status.progress_percent = 95;
    
    if (g_ota_status.type == OTA_TYPE_FIRMWARE) {
        // Finalize OTA update
        ret = esp_ota_end(g_ota_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(ret));
            snprintf(g_ota_status.error_message, sizeof(g_ota_status.error_message), 
                    "OTA end failed: %s", esp_err_to_name(ret));
            g_ota_status.state = OTA_STATE_ERROR;
            return ret;
        }
        
        // Set boot partition to the new firmware
        ret = esp_ota_set_boot_partition(g_update_partition);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(ret));
            snprintf(g_ota_status.error_message, sizeof(g_ota_status.error_message), 
                    "Set boot partition failed: %s", esp_err_to_name(ret));
            g_ota_status.state = OTA_STATE_ERROR;
            return ret;
        }
        
        ESP_LOGI(TAG, "Boot partition set to: %s", g_update_partition->label);
        
        // Update boot count for rollback detection
        update_boot_count(0); // Reset boot count for new firmware
        
    } else {
        // For filesystem updates, we need to ensure data is flushed
        ESP_LOGI(TAG, "Filesystem update completed - flushing data");
        
        // Ensure all data is written to flash
        if (g_update_partition) {
            // The data should already be written via esp_partition_write calls
            ESP_LOGI(TAG, "Filesystem data written to partition: %s", g_update_partition->label);
        }
        
        ESP_LOGI(TAG, "Filesystem update completed - reboot required");
        g_ota_status.reboot_required = true;
    }
    
    g_ota_status.state = OTA_STATE_SUCCESS;
    g_ota_status.progress_percent = 100;
    
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
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, data, len);
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    if (memcmp(hash, expected_hash, 32) == 0) return ESP_OK;
    return ESP_FAIL;
}

esp_err_t ota_auto_rollback(void) {
    ESP_LOGW(TAG, "Automatic rollback initiated");
    
    if (g_ota_status.type == OTA_TYPE_FIRMWARE) {
        // Cancel current OTA operation
        if (g_ota_handle != 0) {
            esp_ota_abort(g_ota_handle);
            g_ota_handle = 0;
        }
        
        // Get currently running partition
        const esp_partition_t* running_partition = esp_ota_get_running_partition();
        if (running_partition != NULL) {
            // Set boot partition back to current running partition
            esp_err_t ret = esp_ota_set_boot_partition(running_partition);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Rollback successful, boot partition restored to: %s", 
                        running_partition->label);
            } else {
                ESP_LOGE(TAG, "Rollback failed: %s", esp_err_to_name(ret));
                return ret;
            }
        }
    }
    
    // Clean up crypto context if initialized
    if (g_crypto_init) {
        mbedtls_sha256_free(&g_sha256_ctx);
        g_crypto_init = false;
    }
    
    // Reset status
    g_ota_status.state = OTA_STATE_ERROR;
    snprintf(g_ota_status.error_message, sizeof(g_ota_status.error_message), 
            "Update rolled back due to verification failure");
    
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
