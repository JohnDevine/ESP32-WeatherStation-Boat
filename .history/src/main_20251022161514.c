/**
 * @file main.c
 * @brief ESP32 Access Point + Captive Portal + NVS + SPIFFS Application
 * @version 1.0.0
 * @date 2025-10-18
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

// =============================
// Includes
// =============================
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "version.h"
#include "nvs_utils.h"
#include "wifi_ap.h"
#include "dns_server.h"
#include "web_server.h"
#include "SystemMetrics.h"
#include "gateway.h"
#include "node.h"

// =============================
// Function Prototypes
// =============================
static void init_system(void);
static void init_config_hardware(void);
static bool wait_for_boot_button(void);

// =============================
// Constants & Definitions
// =============================
// Register main.c version
REGISTER_VERSION(Main, "1.0.0", "2025-10-18");
static const char *TAG = "MAIN";

// Boot button configuration (GPIO 0 on ESP32 DevKit V1)
#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define BOOT_BUTTON_WAIT_TIME_MS 10000  // 10 seconds
#define BOOT_BUTTON_POLL_INTERVAL_MS 100  // 100ms polling

// =============================
// Function Definitions
// =============================

/**
 * @brief Initialize core system components (always required)
 * 
 * Initializes NVS storage and SystemMetrics. These components are required
 * regardless of the execution path (config mode or normal operation).
 */
static void init_system(void) {
    ESP_LOGI(TAG, "Initializing core system components...");

    // Initialize NVS storage (required first for SystemMetrics)
    ESP_ERROR_CHECK(nvs_utils_init());

    // Initialize SystemMetrics FIRST (includes boot count tracking)
    // This ensures boot count is updated even if other features are bypassed
    ESP_LOGI(TAG, "About to initialize SystemMetrics...");
    if (!system_metrics_init()) {
        ESP_LOGW(TAG, "SystemMetrics initialization failed");
    } else {
        ESP_LOGI(TAG, "SystemMetrics initialization successful");
    }

    ESP_LOGI(TAG, "Core system components initialized successfully");
}

/**
 * @brief Wait for boot button press with timeout
 * 
 * @return true if boot button was pressed within timeout period
 * @return false if timeout occurred without button press
 */
static bool wait_for_boot_button(void) {
    ESP_LOGI(TAG, "Waiting %d seconds for boot button press (GPIO %d)...", 
             BOOT_BUTTON_WAIT_TIME_MS / 1000, BOOT_BUTTON_GPIO);
    
    // Configure boot button GPIO as input with pull-up
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Poll for button press (active LOW)
    int poll_count = BOOT_BUTTON_WAIT_TIME_MS / BOOT_BUTTON_POLL_INTERVAL_MS;
    for (int i = 0; i < poll_count; i++) {
        if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
            ESP_LOGI(TAG, "Boot button pressed! Entering configuration mode...");
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(BOOT_BUTTON_POLL_INTERVAL_MS));
    }

    ESP_LOGI(TAG, "Boot button NOT pressed.");
    return false;
}

/**
 * @brief Initialize configuration hardware components (WiFi AP, DNS, Web server)
 * 
 * Initializes components needed for configuration mode: SPIFFS filesystem,
 * WiFi Access Point, DNS server, and HTTP web server.
 */
static void init_config_hardware(void) {
    ESP_LOGI(TAG, "Initializing configuration hardware components...");

    // Initialize SPIFFS file system
    ESP_ERROR_CHECK(web_server_init_spiffs());

    // Initialize WiFi Access Point
    ESP_ERROR_CHECK(wifi_ap_init());

    // Start DNS server for captive portal
    ESP_ERROR_CHECK(dns_server_start());

    // Start HTTP web server
    ESP_ERROR_CHECK(web_server_start());

    ESP_LOGI(TAG, "All configuration hardware components initialized successfully");
}

// =============================
// ESP-IDF Entry Point
// =============================
void app_main(void) {
    ESP_LOGI(TAG, "ESP32 Access Point + Captive Portal starting...");
    
    // Print project version information
    print_version_info();

    // Initialize core system components (always required)
    init_system();

    // Wait for boot button press to determine execution path
    if (wait_for_boot_button()) {
        // Boot button was pressed - enter configuration mode
        ESP_LOGI(TAG, "Entering configuration mode...");
        
        // Initialize configuration hardware components
        init_config_hardware();

        ESP_LOGI(TAG, "=== Configuration Mode Ready ===");
        ESP_LOGI(TAG, "WiFi AP: %s", AP_SSID);
        ESP_LOGI(TAG, "IP Address: 192.168.4.1");
        ESP_LOGI(TAG, "Web Interface: http://192.168.4.1/");
        ESP_LOGI(TAG, "DNS Server: Running on port 53");
        ESP_LOGI(TAG, "HTTP Server: Running on port 80");
        ESP_LOGI(TAG, "=====================================");

        // Main configuration mode loop (keep alive)
        while (1) {
            // Log system status every 30 seconds
            ESP_LOGI(TAG, "System status - WiFi AP: %s, DNS: %s, Web: %s",
                    wifi_ap_is_running() ? "Running" : "Stopped",
                    dns_server_is_running() ? "Running" : "Stopped",
                    web_server_is_running() ? "Running" : "Stopped");

            vTaskDelay(pdMS_TO_TICKS(30000));  // 30 seconds
        }
        
        // Note: If we ever exit the loop, reboot the system
        ESP_LOGI(TAG, "Configuration mode complete - rebooting...");
        esp_restart();
        
    } else {
        // Boot button was NOT pressed - normal operation mode
        ESP_LOGI(TAG, "Boot button NOT pressed - entering normal operation mode");

        // Always initialize configuration hardware for web interface access
        init_config_hardware();

        // Now check device role and fork main processing logic
        ESP_LOGI(TAG, "Checking device role for main processing logic...");

        // Load device role from NVS
        uint8_t device_role = DEVICE_ROLE_RESPONDER; // Default fallback
        if (nvs_load_device_role(&device_role) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to load device role, using default (Responder)");
            device_role = DEVICE_ROLE_RESPONDER;
        }

        // Fork main processing logic based on device role
        if (device_role == DEVICE_ROLE_GATEWAY) {
            ESP_LOGI(TAG, "Device role: Gateway - initializing gateway mode...");
            ESP_LOGI(TAG, "=== Gateway Mode Starting ===");

            // Initialize gateway functionality
            if (gateway_init() == ESP_OK) {
                ESP_LOGI(TAG, "Gateway initialization successful");

                // Main gateway processing loop
                while (1) {
                    gateway_main();
                    vTaskDelay(pdMS_TO_TICKS(1000));  // 1 second delay between iterations
                }
            } else {
                ESP_LOGE(TAG, "Gateway initialization failed - rebooting...");
                vTaskDelay(pdMS_TO_TICKS(5000));  // Wait 5 seconds before reboot
                esp_restart();
            }

        } else if (device_role == DEVICE_ROLE_RESPONDER) {
            ESP_LOGI(TAG, "Device role: Responder/Node - initializing node mode...");
            ESP_LOGI(TAG, "=== Node Mode Starting ===");

            // Initialize node functionality
            if (node_init() == ESP_OK) {
                ESP_LOGI(TAG, "Node initialization successful");

                // Main node processing loop
                while (1) {
                    node_main();
                    vTaskDelay(pdMS_TO_TICKS(1000));  // 1 second delay between iterations
                }
            } else {
                ESP_LOGE(TAG, "Node initialization failed - rebooting...");
                vTaskDelay(pdMS_TO_TICKS(5000));  // Wait 5 seconds before reboot
                esp_restart();
            }

        } else {
            ESP_LOGE(TAG, "Invalid device role: %d - rebooting...", device_role);
            vTaskDelay(pdMS_TO_TICKS(5000));  // Wait 5 seconds before reboot
            esp_restart();
        }
    }
}