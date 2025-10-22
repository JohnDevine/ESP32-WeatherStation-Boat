/**
 * @file ota_manager.h
 * @brief OTA Manager API for ESP32 firmware and filesystem updates
 * @version 1.0.0
 * @date 2025-10-19
 * @author John Devine <john.h.devine@gmail.com>
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

// =============================
// Includes
// =============================
#include <stdbool.h>
#include "esp_err.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"

// =============================
// Constants & Definitions
// =============================
#define OTA_CHUNK_SIZE          8192
#define OTA_MAX_FIRMWARE_SIZE   (1280 * 1024)  // 1.25MB
#define OTA_MAX_FILESYSTEM_SIZE (1472 * 1024)  // ~1.44MB
#define OTA_MAX_BOOT_ATTEMPTS   3
#define OTA_HASH_LEN            32              // SHA256 hash length
#define OTA_HASH_STR_LEN        65              // SHA256 hex string length

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_UPLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_FLASHING,
    OTA_STATE_SUCCESS,
    OTA_STATE_ERROR
} ota_state_t;

typedef enum {
    OTA_TYPE_FIRMWARE = 0,
    OTA_TYPE_FILESYSTEM
} ota_type_t;

typedef struct {
    ota_state_t state;
    ota_type_t type;
    size_t total_size;
    size_t uploaded_size;
    uint8_t progress_percent;
    char error_message[128];
    bool backup_available;
    bool backup_created;
    bool backup_skipped;
    bool reboot_required;
} ota_status_t;

typedef struct {
    bool create_backup;
    bool verify_crypto;
    char expected_hash[OTA_HASH_STR_LEN];
    ota_type_t update_type;
} ota_config_t;

// =============================
// Function Prototypes
// =============================

/**
 * @brief Initialize OTA manager system
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ota_manager_init(void);

/**
 * @brief Register OTA HTTP endpoints with web server
 * @param server HTTP server handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ota_register_endpoints(httpd_handle_t server);

/**
 * @brief Start OTA update process with optional backup
 * @param config OTA configuration including backup preferences
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ota_start_update(const ota_config_t* config);

/**
 * @brief Process uploaded OTA data chunk
 * @param data Data chunk to process
 * @param size Size of data chunk
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ota_process_chunk(const uint8_t* data, size_t size);

/**
 * @brief Finalize OTA update process
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ota_finalize_update(void);

/**
 * @brief Get current OTA status
 * @param status Pointer to status structure to fill
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ota_get_status(ota_status_t* status);

/**
 * @brief Create backup of current firmware/filesystem (optional)
 * @param type Type of backup to create
 * @return ESP_OK on success, error code on failure, ESP_ERR_NOT_SUPPORTED if disabled
 */
esp_err_t ota_create_backup(ota_type_t type);

/**
 * @brief Check if backup is available for download
 * @param type Type of backup to check
 * @return true if backup available, false otherwise
 */
bool ota_is_backup_available(ota_type_t type);

/**
 * @brief Verify uploaded data using SHA256 (optional)
 * @param data Data buffer to verify
 * @param len Length of data buffer
 * @param expected_hash Expected SHA256 hash (32 bytes), NULL to skip verification
 * @return ESP_OK if valid or verification skipped, ESP_FAIL if invalid
 */
esp_err_t ota_verify_crypto(const uint8_t* data, size_t len, const uint8_t* expected_hash);

/**
 * @brief Perform automatic rollback on failure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ota_auto_rollback(void);

/**
 * @brief Set backup preference for future updates
 * @param enable_backup true to enable automatic backup, false to disable
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ota_set_backup_preference(bool enable_backup);

/**
 * @brief Get current backup preference
 * @return true if backup enabled by default, false otherwise
 */
bool ota_get_backup_preference(void);

/**
 * @brief Reboot system after successful OTA update
 * @return Does not return
 */
void ota_reboot_system(void);

/**
 * @brief Get partition information for backup purposes
 * @param type Type of partition to get info for
 * @param partition Pointer to store partition info
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ota_get_partition_info(ota_type_t type, const esp_partition_t** partition);

#endif // OTA_MANAGER_H