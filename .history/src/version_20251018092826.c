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
    ESP_LOGI(TAG, "%s v%s", PROJECT_NAME, PROJECT_VERSION);
    ESP_LOGI(TAG, "Built on %s at %s", PROJECT_BUILD_DATE, PROJECT_BUILD_TIME);
    
    // Additional component versions can be printed here if needed
    // or by the components themselves during initialization
}
