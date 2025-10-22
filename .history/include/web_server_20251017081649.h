/**
 * @file web_server.h
 * @brief HTTP web server for captive portal interface
 * @version 0.1
 * @date 2025-10-17
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================
// Constants & Definitions
// =============================
#define WEB_SERVER_PORT 80
#define SPIFFS_BASE_PATH "/data"

// =============================
// Function Prototypes
// =============================

/**
 * @brief Initialize and start HTTP web server
 *
 * Sets up HTTP server with URI handlers for:
 * - Static file serving from SPIFFS
 * - Configuration API endpoints
 * - Captive portal redirects
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t web_server_start(void);

/**
 * @brief Stop HTTP web server
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t web_server_stop(void);

/**
 * @brief Check if web server is running
 *
 * @return true if server is running, false otherwise
 */
bool web_server_is_running(void);

/**
 * @brief Initialize SPIFFS file system
 *
 * Mounts SPIFFS partition for serving web files
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t web_server_init_spiffs(void);

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H