/**
 * @file main.c
 * @brief ESP32 Access Point + Captive Portal + NVS + SPIFFS Application
 * @version 1.0.0
 * @date 2025-10-18
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

// =============================
// Function Prototypes
// =============================
static void init_hardware(void);

// =============================
// Includes
// =============================
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "version.h"
#include "nvs_utils.h"
#include "wifi_ap.h"
#include "dns_server.h"
#include "web_server.h"
#include "SystemMetrics.h"

// =============================
// Constants & Definitions
// =============================
// Register main.c version
REGISTER_VERSION(Main, "1.0.0", "2025-10-18");
static const char *TAG = "MAIN";

// =============================
// Function Definitions
// =============================

static void init_hardware(void) {
    ESP_LOGI(TAG, "Initializing hardware components...");

    // Initialize NVS storage
    ESP_ERROR_CHECK(nvs_utils_init());

    // Initialize SystemMetrics (includes boot count tracking)
    if (!system_metrics_init()) {
        ESP_LOGW(TAG, "SystemMetrics initialization failed");
    }

    // Initialize SPIFFS file system
    ESP_ERROR_CHECK(web_server_init_spiffs());

    // Initialize WiFi Access Point
    ESP_ERROR_CHECK(wifi_ap_init());

    // Start DNS server for captive portal
    ESP_ERROR_CHECK(dns_server_start());

    // Start HTTP web server
    ESP_ERROR_CHECK(web_server_start());

    ESP_LOGI(TAG, "All hardware components initialized successfully");
}

// =============================
// ESP-IDF Entry Point
// =============================
void app_main(void) {
    ESP_LOGI(TAG, "ESP32 Access Point + Captive Portal starting...");
    
    // Print project version information
    print_version_info();

    // Initialize all hardware components
    init_hardware();

    ESP_LOGI(TAG, "=== System Ready ===");
    ESP_LOGI(TAG, "WiFi AP: %s", AP_SSID);
    ESP_LOGI(TAG, "IP Address: 192.168.4.1");
    ESP_LOGI(TAG, "Web Interface: http://192.168.4.1/");
    ESP_LOGI(TAG, "DNS Server: Running on port 53");
    ESP_LOGI(TAG, "HTTP Server: Running on port 80");
    ESP_LOGI(TAG, "==================");

    // Main application loop (keep alive)
    while (1) {
        // Log system status every 30 seconds
        ESP_LOGI(TAG, "System status - WiFi AP: %s, DNS: %s, Web: %s",
                wifi_ap_is_running() ? "Running" : "Stopped",
                dns_server_is_running() ? "Running" : "Stopped",
                web_server_is_running() ? "Running" : "Stopped");

        vTaskDelay(pdMS_TO_TICKS(30000));  // 30 seconds
    }
}