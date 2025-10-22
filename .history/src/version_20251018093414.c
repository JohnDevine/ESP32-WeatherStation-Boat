/**
 * @file version.c
 * @brief Implementation of version information functions
 * @version 1.0.0
 * @date 2025-10-18
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

#include "version.h"
#include "esp_log.h"

// Register version.c version
REGISTER_VERSION(Version, "1.0.0", "2025-10-18");

static const char* TAG = "VERSION";

/**
 * @brief Print project version information
 * 
 * This function prints the project version and build date
 */
void print_version_info(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "%s v%s", PROJECT_NAME, PROJECT_VERSION);
    ESP_LOGI(TAG, "Built on %s at %s", PROJECT_BUILD_DATE, PROJECT_BUILD_TIME);
    ESP_LOGI(TAG, "Author: John Devine <john.h.devine@gmail.com>");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Component Versions:");
    ESP_LOGI(TAG, "  Main Application: v1.0.0");
    ESP_LOGI(TAG, "  DNS Server: v1.0.0");
    ESP_LOGI(TAG, "  NVS Utils: v1.0.0");
    ESP_LOGI(TAG, "  Web Server: v1.0.0");
    ESP_LOGI(TAG, "  WiFi AP: v1.0.0");
    ESP_LOGI(TAG, "  Version System: v1.0.0");
    ESP_LOGI(TAG, "  SystemMetrics Lib: v1.0.0");
    ESP_LOGI(TAG, "========================================");
}
