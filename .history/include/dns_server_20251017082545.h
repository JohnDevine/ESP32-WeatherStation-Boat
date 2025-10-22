/**
 * @file dns_server.h
 * @brief DNS server for captive portal functionality
 * @version 0.1
 * @date 2025-10-17
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================
// Constants & Definitions
// =============================
#define DNS_SERVER_PORT 53
#define DNS_RESPONSE_IP "192.168.4.1"

// =============================
// Function Prototypes
// =============================

/**
 * @brief Start DNS server for captive portal
 *
 * Creates a UDP socket on port 53 and responds to all DNS queries
 * by redirecting them to the ESP32 IP address (192.168.4.1)
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t dns_server_start(void);

/**
 * @brief Stop DNS server
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t dns_server_stop(void);

/**
 * @brief Check if DNS server is running
 *
 * @return true if DNS server is running, false otherwise
 */
bool dns_server_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // DNS_SERVER_H