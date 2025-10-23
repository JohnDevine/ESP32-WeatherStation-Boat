/**
 * @file gateway.h
 * @brief Gateway functionality module header
 * @version 1.0.0
 * @date 2025-10-22
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

#ifndef GATEWAY_H
#define GATEWAY_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize gateway mode
 *
 * Sets up all necessary components for gateway operation including
 * WiFi connectivity, MQTT client, and data forwarding.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t gateway_init(void);

/**
 * @brief Main gateway processing loop
 *
 * This function contains the main gateway logic for receiving data
 * from nodes and forwarding it via MQTT. This function should be
 * called from the main application loop.
 */
void gateway_main(void);

/**
 * @brief Cleanup gateway resources
 *
 * Frees any resources allocated during gateway operation.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t gateway_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GATEWAY_H