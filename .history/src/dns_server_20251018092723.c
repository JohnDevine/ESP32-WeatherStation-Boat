/**
 * @file dns_server.c
 * @brief DNS server implementation for captive portal
 * @version 1.0.0
 * @date 2025-10-18
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

// =============================
// Includes
// =============================
#include "dns_server.h"
#include "version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include <string.h>

// =============================
// Constants & Definitions
// =============================
// Register dns_server.c version
REGISTER_VERSION(DnsServer, "1.0.0", "2025-10-18");
static const char *TAG = "DNS_SERVER";
static TaskHandle_t dns_task_handle = NULL;
static int dns_socket = -1;
static bool dns_running = false;

#define DNS_MAX_PACKET_SIZE 512
#define DNS_TASK_STACK_SIZE 4096
#define DNS_TASK_PRIORITY 5

// =============================
// Function Prototypes
// =============================
static void dns_server_task(void *pvParameters);

// =============================
// Function Definitions
// =============================

static void dns_server_task(void *pvParameters) {
    char rx_buffer[DNS_MAX_PACKET_SIZE];
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_UDP;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DNS_SERVER_PORT);

    // Create socket
    dns_socket = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (dns_socket < 0) {
        ESP_LOGE(TAG, "Unable to create DNS socket: errno %d", errno);
        dns_running = false;
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "DNS socket created successfully");

    // Bind socket
    int err = bind(dns_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(dns_socket);
        dns_socket = -1;
        dns_running = false;
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "DNS server socket bound to port %d", DNS_SERVER_PORT);

    dns_running = true;

    while (dns_running) {
        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);
        
        int len = recvfrom(dns_socket, rx_buffer, sizeof(rx_buffer) - 1, 0,
                          (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            if (dns_running) {  // Only log error if we're still supposed to be running
                ESP_LOGE(TAG, "DNS recvfrom failed: errno %d", errno);
            }
            break;
        }

        // Get sender's IP address
        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, 
                       addr_str, sizeof(addr_str) - 1);
        }

        ESP_LOGD(TAG, "DNS query received from %s, length: %d", addr_str, len);

        // Create DNS response - redirect all queries to our IP
        if (len >= 12) {  // Minimum DNS header size
            uint8_t response[DNS_MAX_PACKET_SIZE];
            memcpy(response, rx_buffer, len);  // Copy query

            // Set response flags
            response[2] = 0x81;  // Response, no error
            response[3] = 0x80;  // Recursive available

            // Answer count = 1
            response[6] = 0x00;
            response[7] = 0x01;

            // Add answer (A record pointing to our IP: 192.168.4.1)
            int pos = len;
            response[pos++] = 0xC0;  // Pointer to domain name
            response[pos++] = 0x0C;
            response[pos++] = 0x00;  // Type A
            response[pos++] = 0x01;
            response[pos++] = 0x00;  // Class IN
            response[pos++] = 0x01;
            response[pos++] = 0x00;  // TTL (60 seconds)
            response[pos++] = 0x00;
            response[pos++] = 0x00;
            response[pos++] = 0x3C;
            response[pos++] = 0x00;  // Data length
            response[pos++] = 0x04;
            response[pos++] = 192;   // IP: 192.168.4.1
            response[pos++] = 168;
            response[pos++] = 4;
            response[pos++] = 1;

            // Send response
            int send_err = sendto(dns_socket, response, pos, 0,
                                 (struct sockaddr *)&source_addr, sizeof(source_addr));
            if (send_err < 0) {
                ESP_LOGE(TAG, "Error sending DNS response: errno %d", errno);
            } else {
                ESP_LOGD(TAG, "DNS response sent successfully to %s", addr_str);
            }
        }
    }

    // Clean up
    if (dns_socket != -1) {
        ESP_LOGI(TAG, "Shutting down DNS server socket");
        shutdown(dns_socket, 0);
        close(dns_socket);
        dns_socket = -1;
    }
    
    dns_running = false;
    dns_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t dns_server_start(void) {
    if (dns_running) {
        ESP_LOGW(TAG, "DNS server is already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting DNS server task...");
    
    BaseType_t result = xTaskCreate(dns_server_task, "dns_server", 
                                   DNS_TASK_STACK_SIZE, NULL, 
                                   DNS_TASK_PRIORITY, &dns_task_handle);
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create DNS server task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "DNS server task created successfully");
    return ESP_OK;
}

esp_err_t dns_server_stop(void) {
    if (!dns_running) {
        ESP_LOGW(TAG, "DNS server is not running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping DNS server...");
    
    dns_running = false;

    // Close socket to break out of recvfrom
    if (dns_socket != -1) {
        shutdown(dns_socket, SHUT_RDWR);
        close(dns_socket);
        dns_socket = -1;
    }

    // Wait for task to complete
    if (dns_task_handle != NULL) {
        // Give task some time to clean up
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // If task is still running, delete it
        if (dns_task_handle != NULL) {
            vTaskDelete(dns_task_handle);
            dns_task_handle = NULL;
        }
    }

    ESP_LOGI(TAG, "DNS server stopped successfully");
    return ESP_OK;
}

bool dns_server_is_running(void) {
    return dns_running;
}