/**
 * @file node.h
 * @brief Node functionality module header
 * @version 1.0.0
 * @date 2025-10-22
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

#ifndef NODE_H
#define NODE_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize node mode
 *
 * Sets up all necessary components for node operation including
 * sensor initialization, ESP-NOW setup, and data collection.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t node_init(void);

/**
 * @brief Main node processing loop
 *
 * This function contains the main node logic for collecting sensor
 * data and sending it to the gateway. This function should be
 * called from the main application loop.
 */
void node_main(void);

/**
 * @brief Cleanup node resources
 *
 * Frees any resources allocated during node operation.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t node_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // NODE_H