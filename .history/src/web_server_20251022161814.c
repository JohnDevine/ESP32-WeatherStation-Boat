/**
 * @file web_server.c
 * @brief HTTP web server implementation for captive portal
 * @version 1.0.0
 * @date 2025-10-18
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

// =============================
// Includes
// =============================
#include "web_server.h"
#include "nvs_utils.h"
#include "version.h"
#include "SystemMetrics.h"
#include "ota_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "esp_spiffs.h"
#include <errno.h>
#include "esp_log.h"

// =============================
// Function Prototypes
// =============================
static esp_err_t file_get_handler(httpd_req_t *req);
static esp_err_t save_config_handler(httpd_req_t *req);
static esp_err_t get_config_handler(httpd_req_t *req);
static esp_err_t get_metric_handler(httpd_req_t *req);
static esp_err_t get_version_info_handler(httpd_req_t *req);
static esp_err_t captive_portal_redirect_handler(httpd_req_t *req);
static esp_err_t ota_api_get_handler(httpd_req_t *req);
// Reboot task for delayed filesystem reboot
static void reboot_task(void *pvParameter) {
    static const char* REBOOT_TAG = "REBOOT_TASK";
    ESP_LOGI(REBOOT_TAG, "Reboot task starting - waiting 3 seconds");
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(REBOOT_TAG, "Rebooting system now...");
    esp_restart();
}

// =============================
// Constants & Definitions
// =============================
// Register web_server.c version
REGISTER_VERSION(WebServer, "1.0.0", "2025-10-18");

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server_handle = NULL;

// =============================
// Function Definitions
// =============================

esp_err_t web_server_init_spiffs(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS...");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE_PATH,
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret == ESP_FAIL) {
        ESP_LOGW(TAG, "Failed to mount SPIFFS, formatting...");
        ret = esp_spiffs_format(NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to format SPIFFS: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = esp_vfs_spiffs_register(&conf);
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPIFFS partition size: total=%zu bytes, used=%zu bytes", total, used);
    return ESP_OK;
}

static esp_err_t file_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "File request: %s", req->uri);
    ESP_LOGI(TAG, "Request method: %s", req->method == HTTP_GET ? "GET" : "OTHER");

    char filepath[512];  // Increased buffer size
    const char *uri = req->uri;

    // Handle root path - serve index.html
    if (strcmp(uri, "/") == 0) {
        uri = "/index.html";
        ESP_LOGI(TAG, "Root request, serving index.html");
    }

    // Build full file path with proper bounds checking
    size_t base_path_len = strlen(SPIFFS_BASE_PATH);
    size_t uri_len = strlen(uri);
    
    if (base_path_len + uri_len + 1 >= sizeof(filepath)) {
        ESP_LOGW(TAG, "URI too long: %s", uri);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "URI too long");
        return ESP_FAIL;
    }

    int written = snprintf(filepath, sizeof(filepath), "%s%s", SPIFFS_BASE_PATH, uri);
    if (written >= (int)sizeof(filepath) || written < 0) {
        ESP_LOGW(TAG, "Path construction failed for URI: %s", uri);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid URI");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Full file path: %s", filepath);

    // Check if file exists
    struct stat st;
    if (stat(filepath, &st) != 0) {
        ESP_LOGW(TAG, "File not found: %s (errno: %d)", filepath, errno);
        // Redirect to captive portal for missing files
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "File exists, size: %ld bytes", st.st_size);

    // Open file
    FILE *fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to open file: %s (errno: %d)", filepath, errno);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "File opened successfully");

    // Set content type based on file extension
    const char *mime_type = "text/plain";
    if (strstr(filepath, ".html")) {
        mime_type = "text/html";
        ESP_LOGI(TAG, "Serving HTML file");
    } else if (strstr(filepath, ".css")) {
        mime_type = "text/css";
        ESP_LOGI(TAG, "Serving CSS file");
    } else if (strstr(filepath, ".js")) {
        mime_type = "application/javascript";
        ESP_LOGI(TAG, "Serving JavaScript file");
    } else if (strstr(filepath, ".ico")) {
        mime_type = "image/x-icon";
        ESP_LOGI(TAG, "Serving icon file");
    }

    httpd_resp_set_type(req, mime_type);
    ESP_LOGI(TAG, "Set content type: %s", mime_type);

    // Read and send file in chunks
    char buffer[1024];
    size_t bytes_read;
    size_t total_sent = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fd)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, bytes_read) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send file chunk");
            fclose(fd);
            httpd_resp_sendstr_chunk(req, NULL);  // End chunked response
            return ESP_FAIL;
        }
        total_sent += bytes_read;
    }

    fclose(fd);
    httpd_resp_sendstr_chunk(req, NULL);  // End chunked response
    ESP_LOGI(TAG, "File sent successfully, total bytes: %zu", total_sent);
    return ESP_OK;
}

/**
 * @brief Handle GET /api/ota
 * 
 * Returns JSON status information
 */
static esp_err_t ota_api_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "OTA GET request: %s", req->uri);

    // Return JSON status (backup functionality removed)
    ota_status_t status;
    if (ota_get_status(&status) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get status");
        return ESP_FAIL;
    }

    // Get current running partition info
    const esp_partition_t* running_partition = esp_ota_get_running_partition();
    const char* partition_name = "Unknown";
    if (running_partition) {
        partition_name = running_partition->label;
    }

    char resp[512];
    snprintf(resp, sizeof(resp),
             "{\"state\":%d,\"type\":%d,\"progress\":%d,\"backup_available\":%s,\"backup_created\":%s,\"backup_skipped\":%s,\"current_partition\":\"%s\",\"error\":\"%s\"}",
             status.state, status.type, status.progress_percent,
             status.backup_available ? "true" : "false",
             status.backup_created ? "true" : "false",
             status.backup_skipped ? "true" : "false",
             partition_name,
             status.error_message);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/**
 * @brief Handle POST /api/ota
 * Accepts multipart/form-data with fields: type, hash, file
 */
static esp_err_t ota_api_post_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "OTA POST upload received");

    int total = req->content_len;
    if (total <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty upload");
        return ESP_FAIL;
    }

    const int MAX_UPLOAD = 4 * 1024 * 1024; // 4MB
    if (total > MAX_UPLOAD) {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Upload too large");
        return ESP_FAIL;
    }

    // Use a larger buffer for better multipart parsing
    const int HEADER_BUF_SIZE = 4096;
    char *header_buf = malloc(HEADER_BUF_SIZE);
    if (!header_buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    // Read enough data to parse the multipart headers
    int received = 0;
    int header_read = 0;
    while (header_read < HEADER_BUF_SIZE - 1 && received < total) {
        int to_read = (HEADER_BUF_SIZE - 1 - header_read > total - received) ? 
                      total - received : HEADER_BUF_SIZE - 1 - header_read;
        int r = httpd_req_recv(req, header_buf + header_read, to_read);
        if (r <= 0) {
            free(header_buf);
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Receive error");
            return ESP_FAIL;
        }
        header_read += r;
        received += r;
        
        // Check if we have found the file data start
        header_buf[header_read] = '\0';
        if (strstr(header_buf, "filename=\"") && strstr(header_buf, "\r\n\r\n")) {
            break;
        }
    }

    ESP_LOGI(TAG, "Read %d bytes for header parsing", header_read);

    // Parse configuration from headers
    ota_config_t cfg = {0};
    cfg.create_backup = false; // Always skip backup (functionality removed)
    cfg.verify_crypto = false; // Disable crypto for streaming
    cfg.expected_hash[0] = '\0';
    cfg.update_type = OTA_TYPE_FIRMWARE;

    // Extract 'type' field
    char* tptr = strstr(header_buf, "name=\"type\"");
    if (tptr) {
        char *val = strstr(tptr, "\r\n\r\n");
        if (val && val < header_buf + header_read) {
            val += 4;
            char *end = strstr(val, "\r\n");
            if (end && end < header_buf + header_read) {
                char v[32];
                int len = end - val;
                if (len > 0 && len < (int)sizeof(v)) {
                    memcpy(v, val, len);
                    v[len] = '\0';
                    if (strcmp(v, "filesystem") == 0) cfg.update_type = OTA_TYPE_FILESYSTEM;
                }
            }
        }
    }

    // skipBackup processing removed - backup is always disabled

    // Find start of file data by looking for the filename field
    char *filename_start = strstr(header_buf, "filename=\"");
    if (!filename_start) {
        free(header_buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No file in upload");
        return ESP_FAIL;
    }

    // Find the actual file data start after the filename section
    char *file_data_start = strstr(filename_start, "\r\n\r\n");
    if (!file_data_start) {
        free(header_buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Malformed upload");
        return ESP_FAIL;
    }
    file_data_start += 4; // Skip past \r\n\r\n

    // Calculate how much file data is in the header buffer
    int header_size = file_data_start - header_buf;
    int first_data_size = header_read - header_size;

    ESP_LOGI(TAG, "Header size: %d, First data chunk: %d bytes", header_size, first_data_size);

    // Start OTA update
    // Backup preference setting removed
    esp_err_t ret = ota_start_update(&cfg);
    if (ret != ESP_OK) {
        free(header_buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA start failed");
        return ESP_FAIL;
    }

    // Process first chunk of file data if any
    if (first_data_size > 0) {
        ESP_LOGI(TAG, "Processing first chunk: %d bytes", first_data_size);
        ret = ota_process_chunk((const uint8_t*)file_data_start, first_data_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to process first chunk");
            ota_auto_rollback();
            free(header_buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            return ESP_FAIL;
        }
    }

    // Now stream remaining data
    const int CHUNK_SIZE = 2048;
    char *data_buf = malloc(CHUNK_SIZE);
    if (!data_buf) {
        ota_auto_rollback();
        free(header_buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    // Calculate remaining bytes to process
    int remaining = total - received;
    ESP_LOGI(TAG, "Streaming remaining %d bytes", remaining);

    while (remaining > 0) {
        int to_recv = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        int r = httpd_req_recv(req, data_buf, to_recv);
        if (r <= 0) {
            ESP_LOGE(TAG, "Receive error while streaming");
            ota_auto_rollback();
            free(data_buf);
            free(header_buf);
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Receive error");
            return ESP_FAIL;
        }
        received += r;
        remaining -= r;

        // Process this chunk, but check for boundary at the end
        int process_size = r;
        if (remaining == 0) {
            // Last chunk - look for boundary to exclude it
            for (int i = 0; i < r - 5; i++) {
                if (data_buf[i] == '\r' && data_buf[i+1] == '\n' && 
                    data_buf[i+2] == '-' && data_buf[i+3] == '-') {
                    process_size = i;
                    break;
                }
            }
        }

        if (process_size > 0) {
            ret = ota_process_chunk((const uint8_t*)data_buf, process_size);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to process data chunk");
                ota_auto_rollback();
                free(data_buf);
                free(header_buf);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
                return ESP_FAIL;
            }
        }
    }

    // Finalize OTA update
    ESP_LOGI(TAG, "Finalizing OTA update");
    ret = ota_finalize_update();
    
    free(data_buf);
    free(header_buf);

    if (ret == ESP_OK) {
        // Get final status to check if reboot is required
        ota_status_t status;
        ota_get_status(&status);
        
        httpd_resp_set_type(req, "application/json");
        
        if (status.reboot_required) {
            // Both firmware and filesystem updates require reboot - notify client and schedule reboot
            const char* update_type = (status.type == OTA_TYPE_FILESYSTEM) ? "Filesystem" : "Firmware";
            char response[256];
            snprintf(response, sizeof(response), 
                "{\"status\":\"ok\",\"reboot\":true,\"message\":\"%s updated, rebooting in 3 seconds\"}", 
                update_type);
            httpd_resp_send(req, response, -1);
            ESP_LOGI(TAG, "OTA %s update completed - scheduling reboot task", update_type);
            
            // Create a task to handle the delayed reboot
            BaseType_t result = xTaskCreate(reboot_task, "reboot_task", 2048, NULL, 5, NULL);
            if (result != pdPASS) {
                ESP_LOGE(TAG, "Failed to create reboot task, rebooting immediately");
                esp_restart();
            }
        } else {
            // This shouldn't happen anymore, but keep as fallback
            httpd_resp_send(req, "{\"status\":\"ok\",\"message\":\"Update completed successfully\"}", -1);
            ESP_LOGI(TAG, "OTA update completed successfully (no reboot required)");
        }
    } else {
        ESP_LOGE(TAG, "OTA finalize failed: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA finalize failed");
    }

    return ESP_OK;
}

static esp_err_t save_config_handler(httpd_req_t *req) {
    int remaining = req->content_len;
    if (remaining <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty content");
        return ESP_FAIL;
    }

    // Cap incoming content to a reasonable size to avoid OOM
    const size_t MAX_CONTENT = 4096;
    size_t to_alloc = (size_t)remaining + 1;
    if (to_alloc > MAX_CONTENT) {
        to_alloc = MAX_CONTENT;
    }

    char *content = (char *)malloc(to_alloc);
    if (!content) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    size_t received = 0;
    while (remaining > 0 && received < to_alloc - 1) {
        int chunk = remaining > 1024 ? 1024 : remaining;
        int ret = httpd_req_recv(req, content + received, chunk);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            free(content);
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }

    // Null-terminate
    content[received] = '\0';

    ESP_LOGI(TAG, "Received config data: %s", content);

    // Parse JSON-like data (simple string parsing)
    char mac_address[MAC_ADDR_STR_LEN] = "";
    char ip_address[IP_ADDR_STR_LEN] = "";
    char password[WIFI_PASS_MAX_LEN] = "";
    char active_key[33] = "";     // 32 hex chars + null terminator
    char pending_key[33] = "";
    uint32_t boot_count = 0;
    uint8_t device_role = DEVICE_ROLE_RESPONDER;

    // Extract macAddress
    char *mac_start = strstr(content, "\"macAddress\":\"");
    if (mac_start) {
        mac_start += 14;
        char *mac_end = strchr(mac_start, '"');
        if (mac_end && (mac_end - mac_start) < sizeof(mac_address)) {
            strncpy(mac_address, mac_start, mac_end - mac_start);
            mac_address[mac_end - mac_start] = '\0';
        }
    }

    // Extract ipAddress
    char *ip_start = strstr(content, "\"ipAddress\":\"");
    if (ip_start) {
        ip_start += 13;
        char *ip_end = strchr(ip_start, '"');
        if (ip_end && (ip_end - ip_start) < sizeof(ip_address)) {
            strncpy(ip_address, ip_start, ip_end - ip_start);
            ip_address[ip_end - ip_start] = '\0';
        }
    }

    // Extract password
    char *pass_start = strstr(content, "\"password\":\"");
    if (pass_start) {
        pass_start += 12;
        char *pass_end = strchr(pass_start, '"');
        if (pass_end && (pass_end - pass_start) < sizeof(password)) {
            strncpy(password, pass_start, pass_end - pass_start);
            password[pass_end - pass_start] = '\0';
        }
    }

    // Extract activeKey
    char *active_start = strstr(content, "\"activeKey\":\"");
    if (active_start) {
        active_start += 13;
        char *active_end = strchr(active_start, '"');
        if (active_end && (active_end - active_start) < sizeof(active_key)) {
            strncpy(active_key, active_start, active_end - active_start);
            active_key[active_end - active_start] = '\0';
        }
    }

    // Extract pendingKey
    char *pending_start = strstr(content, "\"pendingKey\":\"");
    if (pending_start) {
        pending_start += 14;
        char *pending_end = strchr(pending_start, '"');
        if (pending_end && (pending_end - pending_start) < sizeof(pending_key)) {
            strncpy(pending_key, pending_start, pending_end - pending_start);
            pending_key[pending_end - pending_start] = '\0';
        }
    }

    // Extract bootCount
    char *boot_start = strstr(content, "\"bootCount\":");
    if (boot_start) {
        boot_start += 12;
        char *boot_end = strpbrk(boot_start, ",}");
        if (boot_end) {
            char boot_str[32];
            int len = boot_end - boot_start;
            if (len < sizeof(boot_str)) {
                strncpy(boot_str, boot_start, len);
                boot_str[len] = '\0';
                boot_count = (uint32_t)atol(boot_str);
            }
        }
    }

    // Extract deviceRole
    char *role_start = strstr(content, "\"deviceRole\":");
    if (role_start) {
        role_start += 13;
        char *role_end = strpbrk(role_start, ",}");
        if (role_end) {
            char role_str[8];
            int len = role_end - role_start;
            if (len < sizeof(role_str)) {
                strncpy(role_str, role_start, len);
                role_str[len] = '\0';
                device_role = (uint8_t)atoi(role_str);
                // Validate role value
                if (device_role != DEVICE_ROLE_GATEWAY && device_role != DEVICE_ROLE_RESPONDER) {
                    ESP_LOGW(TAG, "Invalid device role %d, defaulting to responder", device_role);
                    device_role = DEVICE_ROLE_RESPONDER;
                }
            }
        }
    }

    // Extract bridge WiFi configuration
    char bridge_ssid[BRIDGE_SSID_MAX_LEN] = "";
    char bridge_password[BRIDGE_PASS_MAX_LEN] = "";
    char mqtt_server_ip[MQTT_IP_MAX_LEN] = "";
    uint16_t mqtt_port = MQTT_DEFAULT_PORT;
    char mqtt_username[MQTT_USER_MAX_LEN] = "";
    char mqtt_password[MQTT_PASS_MAX_LEN] = "";
    char mqtt_client_id[MQTT_CLIENT_ID_MAX_LEN] = "";
    uint8_t mqtt_qos = MQTT_DEFAULT_QOS;
    char mqtt_base_topic[MQTT_BASE_TOPIC_MAX_LEN] = "";

    // Extract bridgeSsid
    char *bridge_ssid_start = strstr(content, "\"bridgeSsid\":\"");
    if (bridge_ssid_start) {
        bridge_ssid_start += 14;
        char *bridge_ssid_end = strchr(bridge_ssid_start, '"');
        if (bridge_ssid_end && (bridge_ssid_end - bridge_ssid_start) < sizeof(bridge_ssid)) {
            strncpy(bridge_ssid, bridge_ssid_start, bridge_ssid_end - bridge_ssid_start);
            bridge_ssid[bridge_ssid_end - bridge_ssid_start] = '\0';
        }
    }

    // Extract bridgePassword
    char *bridge_pass_start = strstr(content, "\"bridgePassword\":\"");
    if (bridge_pass_start) {
        bridge_pass_start += 17;
        char *bridge_pass_end = strchr(bridge_pass_start, '"');
        if (bridge_pass_end && (bridge_pass_end - bridge_pass_start) < sizeof(bridge_password)) {
            strncpy(bridge_password, bridge_pass_start, bridge_pass_end - bridge_pass_start);
            bridge_password[bridge_pass_end - bridge_pass_start] = '\0';
        }
    }

    // Extract mqttServerIp
    char *mqtt_ip_start = strstr(content, "\"mqttServerIp\":\"");
    if (mqtt_ip_start) {
        mqtt_ip_start += 16;
        char *mqtt_ip_end = strchr(mqtt_ip_start, '"');
        if (mqtt_ip_end && (mqtt_ip_end - mqtt_ip_start) < sizeof(mqtt_server_ip)) {
            strncpy(mqtt_server_ip, mqtt_ip_start, mqtt_ip_end - mqtt_ip_start);
            mqtt_server_ip[mqtt_ip_end - mqtt_ip_start] = '\0';
        }
    }

    // Extract mqttPort
    char *mqtt_port_start = strstr(content, "\"mqttPort\":");
    if (mqtt_port_start) {
        mqtt_port_start += 11;
        char *mqtt_port_end = strpbrk(mqtt_port_start, ",}");
        if (mqtt_port_end) {
            char mqtt_port_str[8];
            int len = mqtt_port_end - mqtt_port_start;
            if (len < sizeof(mqtt_port_str)) {
                strncpy(mqtt_port_str, mqtt_port_start, len);
                mqtt_port_str[len] = '\0';
                mqtt_port = (uint16_t)atoi(mqtt_port_str);
            }
        }
    }

    // Extract mqttUsername
    char *mqtt_user_start = strstr(content, "\"mqttUsername\":\"");
    if (mqtt_user_start) {
        mqtt_user_start += 16;
        char *mqtt_user_end = strchr(mqtt_user_start, '"');
        if (mqtt_user_end && (mqtt_user_end - mqtt_user_start) < sizeof(mqtt_username)) {
            strncpy(mqtt_username, mqtt_user_start, mqtt_user_end - mqtt_user_start);
            mqtt_username[mqtt_user_end - mqtt_user_start] = '\0';
        }
    }

    // Extract mqttPassword
    char *mqtt_pass_start = strstr(content, "\"mqttPassword\":\"");
    if (mqtt_pass_start) {
        mqtt_pass_start += 16;
        char *mqtt_pass_end = strchr(mqtt_pass_start, '"');
        if (mqtt_pass_end && (mqtt_pass_end - mqtt_pass_start) < sizeof(mqtt_password)) {
            strncpy(mqtt_password, mqtt_pass_start, mqtt_pass_end - mqtt_pass_start);
            mqtt_password[mqtt_pass_end - mqtt_pass_start] = '\0';
        }
    }

    // Extract mqttClientId
    char *mqtt_client_start = strstr(content, "\"mqttClientId\":\"");
    if (mqtt_client_start) {
        mqtt_client_start += 16;
        char *mqtt_client_end = strchr(mqtt_client_start, '"');
        if (mqtt_client_end && (mqtt_client_end - mqtt_client_start) < sizeof(mqtt_client_id)) {
            strncpy(mqtt_client_id, mqtt_client_start, mqtt_client_end - mqtt_client_start);
            mqtt_client_id[mqtt_client_end - mqtt_client_start] = '\0';
        }
    }

    // Extract mqttQos
    char *mqtt_qos_start = strstr(content, "\"mqttQos\":");
    if (mqtt_qos_start) {
        mqtt_qos_start += 10;
        char *mqtt_qos_end = strpbrk(mqtt_qos_start, ",}");
        if (mqtt_qos_end) {
            char mqtt_qos_str[4];
            int len = mqtt_qos_end - mqtt_qos_start;
            if (len < sizeof(mqtt_qos_str)) {
                strncpy(mqtt_qos_str, mqtt_qos_start, len);
                mqtt_qos_str[len] = '\0';
                mqtt_qos = (uint8_t)atoi(mqtt_qos_str);
                // Validate QoS value (0, 1, or 2)
                if (mqtt_qos > 2) {
                    ESP_LOGW(TAG, "Invalid MQTT QoS %d, defaulting to 0", mqtt_qos);
                    mqtt_qos = 0;
                }
            }
        }
    }

    // Extract mqttBaseTopic
    char *mqtt_topic_start = strstr(content, "\"mqttBaseTopic\":\"");
    if (mqtt_topic_start) {
        mqtt_topic_start += 17;
        char *mqtt_topic_end = strchr(mqtt_topic_start, '"');
        if (mqtt_topic_end && (mqtt_topic_end - mqtt_topic_start) < sizeof(mqtt_base_topic)) {
            strncpy(mqtt_base_topic, mqtt_topic_start, mqtt_topic_end - mqtt_topic_start);
            mqtt_base_topic[mqtt_topic_end - mqtt_topic_start] = '\0';
        }
    }

    // Save to NVS
    esp_err_t save_result = ESP_OK;
    
    if (strlen(mac_address) > 0) {
        save_result = nvs_store_server_mac(mac_address);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save MAC address");
        }
    }

    if (strlen(ip_address) > 0) {
        save_result = nvs_store_ip_address(ip_address);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save IP address");
        }
    }

    if (strlen(password) > 0) {
        save_result = nvs_store_wifi_password(password);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save WiFi password");
        }
    }

    // Convert hex string keys to binary and save
    if (strlen(active_key) == 32) {  // 16 bytes = 32 hex chars
        uint8_t key_binary[ESPNOW_KEY_LEN];
        bool valid_hex = true;
        
        for (int i = 0; i < ESPNOW_KEY_LEN; i++) {
            char hex_byte[3] = {active_key[i*2], active_key[i*2+1], '\0'};
            char *endptr;
            key_binary[i] = strtol(hex_byte, &endptr, 16);
            if (*endptr != '\0') {
                valid_hex = false;
                break;
            }
        }
        
        if (valid_hex) {
            save_result = nvs_store_espnow_active_key(key_binary);
            if (save_result != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save ESP-NOW active key");
            }
        } else {
            ESP_LOGE(TAG, "Invalid hex format for active key");
        }
    }

    if (strlen(pending_key) == 32) {  // 16 bytes = 32 hex chars
        uint8_t key_binary[ESPNOW_KEY_LEN];
        bool valid_hex = true;
        
        for (int i = 0; i < ESPNOW_KEY_LEN; i++) {
            char hex_byte[3] = {pending_key[i*2], pending_key[i*2+1], '\0'};
            char *endptr;
            key_binary[i] = strtol(hex_byte, &endptr, 16);
            if (*endptr != '\0') {
                valid_hex = false;
                break;
            }
        }
        
        if (valid_hex) {
            save_result = nvs_store_espnow_pending_key(key_binary);
            if (save_result != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save ESP-NOW pending key");
            }
        } else {
            ESP_LOGE(TAG, "Invalid hex format for pending key");
        }
    }

    // Save boot count if provided
    if (boot_count > 0 || strstr(content, "\"bootCount\":0")) {
        save_result = nvs_store_boot_count(boot_count);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save boot count");
        }
    }

    // Save device role if provided
    if (strstr(content, "\"deviceRole\":")) {
        save_result = nvs_store_device_role(device_role);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save device role");
        }
    }

    // Save bridge WiFi configuration
    if (strlen(bridge_ssid) > 0) {
        save_result = nvs_store_bridge_ssid(bridge_ssid);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save bridge SSID");
        }
    }

    if (strlen(bridge_password) > 0) {
        save_result = nvs_store_bridge_password(bridge_password);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save bridge password");
        }
    }

    // Save MQTT configuration
    if (strlen(mqtt_server_ip) > 0) {
        save_result = nvs_store_mqtt_server_ip(mqtt_server_ip);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save MQTT server IP");
        }
    }

    if (strstr(content, "\"mqttPort\":")) {
        save_result = nvs_store_mqtt_port(mqtt_port);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save MQTT port");
        }
    }

    if (strlen(mqtt_username) > 0) {
        save_result = nvs_store_mqtt_username(mqtt_username);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save MQTT username");
        }
    }

    if (strlen(mqtt_password) > 0) {
        save_result = nvs_store_mqtt_password(mqtt_password);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save MQTT password");
        }
    }

    if (strlen(mqtt_client_id) > 0) {
        save_result = nvs_store_mqtt_client_id(mqtt_client_id);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save MQTT client ID");
        }
    }

    if (strstr(content, "\"mqttQos\":")) {
        save_result = nvs_store_mqtt_qos(mqtt_qos);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save MQTT QoS");
        }
    }

    if (strlen(mqtt_base_topic) > 0) {
        save_result = nvs_store_mqtt_base_topic(mqtt_base_topic);
        if (save_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save MQTT base topic");
        }
    }

    // Send response
    const char *response = "{\"message\":\"Configuration saved successfully!\",\"status\":\"success\"}";
    if (save_result != ESP_OK) {
        response = "{\"message\":\"Failed to save configuration\",\"status\":\"error\"}";
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));

    free(content);
    return ESP_OK;
}

static esp_err_t get_config_handler(httpd_req_t *req) {
    // If this is a HEAD request, respond with headers only (no body)
    if (req->method == HTTP_HEAD) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    char mac_address[MAC_ADDR_STR_LEN] = "";
    char ip_address[IP_ADDR_STR_LEN] = "";
    char password[WIFI_PASS_MAX_LEN] = "";
    uint8_t active_key[ESPNOW_KEY_LEN];
    uint8_t pending_key[ESPNOW_KEY_LEN];
    uint32_t boot_count = 0;
    uint8_t device_role = DEVICE_ROLE_RESPONDER;
    char bridge_ssid[BRIDGE_SSID_MAX_LEN] = "";
    char bridge_password[BRIDGE_PASS_MAX_LEN] = "";
    char mqtt_server_ip[MQTT_IP_MAX_LEN] = "";
    uint16_t mqtt_port = MQTT_DEFAULT_PORT;
    char mqtt_username[MQTT_USER_MAX_LEN] = "";
    char mqtt_password[MQTT_PASS_MAX_LEN] = "";
    char mqtt_client_id[MQTT_CLIENT_ID_MAX_LEN] = "";
    uint8_t mqtt_qos = MQTT_DEFAULT_QOS;
    char mqtt_base_topic[MQTT_BASE_TOPIC_MAX_LEN] = "";

    // Load values from NVS
    nvs_load_server_mac(mac_address, sizeof(mac_address));
    nvs_load_ip_address(ip_address, sizeof(ip_address));
    nvs_load_wifi_password(password, sizeof(password));
    nvs_load_espnow_active_key(active_key);
    nvs_load_espnow_pending_key(pending_key);
    nvs_load_boot_count(&boot_count);
    nvs_load_device_role(&device_role);
    nvs_load_bridge_ssid(bridge_ssid, sizeof(bridge_ssid));
    nvs_load_bridge_password(bridge_password, sizeof(bridge_password));
    nvs_load_mqtt_server_ip(mqtt_server_ip, sizeof(mqtt_server_ip));
    nvs_load_mqtt_port(&mqtt_port);
    nvs_load_mqtt_username(mqtt_username, sizeof(mqtt_username));
    nvs_load_mqtt_password(mqtt_password, sizeof(mqtt_password));
    nvs_load_mqtt_client_id(mqtt_client_id, sizeof(mqtt_client_id));
    nvs_load_mqtt_qos(&mqtt_qos);
    nvs_load_mqtt_base_topic(mqtt_base_topic, sizeof(mqtt_base_topic));

    // Convert binary keys to hex strings
    char active_key_hex[33] = "";
    char pending_key_hex[33] = "";
    
    for (int i = 0; i < ESPNOW_KEY_LEN; i++) {
        sprintf(&active_key_hex[i*2], "%02X", active_key[i]);
        sprintf(&pending_key_hex[i*2], "%02X", pending_key[i]);
    }

    // Build JSON response
    char response[2048];
    snprintf(response, sizeof(response),
             "{"
             "\"macAddress\":\"%s\","
             "\"ipAddress\":\"%s\","
             "\"password\":\"%s\","
             "\"activeKey\":\"%s\","
             "\"pendingKey\":\"%s\","
             "\"bootCount\":%lu,"
             "\"deviceRole\":%d,"
             "\"bridgeSsid\":\"%s\","
             "\"bridgePassword\":\"%s\","
             "\"mqttServerIp\":\"%s\","
             "\"mqttPort\":%d,"
             "\"mqttUsername\":\"%s\","
             "\"mqttPassword\":\"%s\","
             "\"mqttClientId\":\"%s\","
             "\"mqttQos\":%d,"
             "\"mqttBaseTopic\":\"%s\""
             "}",
             mac_address, ip_address, password, active_key_hex, pending_key_hex, boot_count, device_role,
             bridge_ssid, bridge_password, mqtt_server_ip, mqtt_port, mqtt_username, mqtt_password,
             mqtt_client_id, mqtt_qos, mqtt_base_topic);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

static esp_err_t get_metric_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET metric request received: %s", req->uri);
    
    // Parse query string to get metric ID
    char query_str[128];
    size_t buf_len = sizeof(query_str);
    
    if (httpd_req_get_url_query_str(req, query_str, buf_len) != ESP_OK) {
        ESP_LOGW(TAG, "No query string provided for metric request");
        // No query string provided
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"Missing metric ID parameter\"}", -1);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Query string: %s", query_str);

    // Extract metric ID from query string
    char metric_id_str[32] = "";
    if (httpd_query_key_value(query_str, "id", metric_id_str, sizeof(metric_id_str)) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"Invalid or missing 'id' parameter\"}", -1);
        return ESP_OK;
    }

    // Convert metric ID to integer
    int metric_id = atoi(metric_id_str);
    ESP_LOGI(TAG, "Requesting metric ID: %d", metric_id);
    
    if (metric_id < 0 || metric_id >= METRIC_COUNT) {
        ESP_LOGW(TAG, "Invalid metric ID: %d (max: %d)", metric_id, METRIC_COUNT-1);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"Invalid metric ID range\"}", -1);
        return ESP_OK;
    }

    // Get the metric value
    const char* metric_value = get_system_metric((system_metric_t)metric_id);
    metric_error_t error_code = get_metric_error();
    
    ESP_LOGI(TAG, "Metric %d result: error=%d, value='%s'", metric_id, error_code, metric_value ? metric_value : "NULL");
    
    // Build JSON response
    char response[256];
    if (error_code == METRIC_OK && metric_value != NULL) {
        // Escape any quotes in the metric value for JSON safety
        char escaped_value[192];
        int j = 0;
        for (int i = 0; metric_value[i] && j < sizeof(escaped_value) - 2; i++) {
            if (metric_value[i] == '"' || metric_value[i] == '\\') {
                escaped_value[j++] = '\\';
            }
            escaped_value[j++] = metric_value[i];
        }
        escaped_value[j] = '\0';
        
        snprintf(response, sizeof(response), 
                "{\"id\":%d,\"value\":\"%s\",\"status\":\"ok\"}", 
                metric_id, escaped_value);
    } else {
        // Determine error message based on error code
        const char* error_msg = "unavailable";
        switch (error_code) {
            case METRIC_ERROR_INVALID_ID:
                error_msg = "invalid_id";
                break;
            case METRIC_ERROR_NOT_AVAILABLE:
                error_msg = "not_available";
                break;
            case METRIC_ERROR_NOT_SUPPORTED:
                error_msg = "not_supported";
                break;
            case METRIC_ERROR_HARDWARE_FAULT:
                error_msg = "hardware_fault";
                break;
            default:
                error_msg = "unavailable";
                break;
        }
        
        snprintf(response, sizeof(response), 
                "{\"id\":%d,\"value\":\"%s\",\"status\":\"error\"}", 
                metric_id, error_msg);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

static esp_err_t get_version_info_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET version info request received");
    
    // Get comprehensive version information
    const char* version_html = get_version_info_string();
    
    // Set response headers
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    // Send the formatted HTML response
    httpd_resp_send(req, version_html, strlen(version_html));
    
    return ESP_OK;
}

static esp_err_t captive_portal_redirect_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Captive portal handler called for URI: %s", req->uri);

    // Handle root requests normally
    if (strcmp(req->uri, "/") == 0) {
        ESP_LOGI(TAG, "Root request, calling file_get_handler");
        return file_get_handler(req);
    }

    // Common captive portal detection URLs
    if (strstr(req->uri, "generate_204") ||
        strstr(req->uri, "connecttest") ||
        strstr(req->uri, "hotspot-detect") ||
        strstr(req->uri, "success.txt") ||
        strstr(req->uri, "ncsi.txt") ||
        strstr(req->uri, "connectivity-check") ||
        strstr(req->uri, "gstatic.com") ||
        strstr(req->uri, "captive.apple.com") ||
        strstr(req->uri, "msftconnecttest.com") ||
        strstr(req->uri, "detectportal")) {

        ESP_LOGI(TAG, "Captive portal detection URL detected: %s", req->uri);

        // Some devices expect a 204 response
        if (strstr(req->uri, "generate_204") || strstr(req->uri, "ncsi.txt")) {
            httpd_resp_set_status(req, "204 No Content");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }

        // Others expect a redirect
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    // For any other unknown requests, redirect to index page
    ESP_LOGW(TAG, "Unknown request URI: %s - redirecting to index", req->uri);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t web_server_start(void) {
    if (server_handle != NULL) {
        ESP_LOGW(TAG, "Web server is already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting web server...");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;
    config.max_uri_handlers = 20;  // Increased from 16 for future expansion

    esp_err_t ret = httpd_start(&server_handle, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error starting server: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register URI handlers
    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = file_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &index_uri);
    ESP_LOGI(TAG, "Registered handler for /");

    httpd_uri_t index_html_uri = {
        .uri = "/index.html",
        .method = HTTP_GET,
        .handler = file_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &index_html_uri);
    ESP_LOGI(TAG, "Registered handler for /index.html");

    httpd_uri_t config_html_uri = {
        .uri = "/configuration.html",
        .method = HTTP_GET,
        .handler = file_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &config_html_uri);
    ESP_LOGI(TAG, "Registered handler for /configuration.html");

    httpd_uri_t information_html_uri = {
        .uri = "/information.html",
        .method = HTTP_GET,
        .handler = file_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &information_html_uri);
    ESP_LOGI(TAG, "Registered handler for /information.html");

    httpd_uri_t ota_html_uri = {
        .uri = "/ota.html",
        .method = HTTP_GET,
        .handler = file_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &ota_html_uri);

    httpd_uri_t css_uri = {
        .uri = "/styles.css",
        .method = HTTP_GET,
        .handler = file_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &css_uri);

    httpd_uri_t js_uri = {
        .uri = "/scripts.js",
        .method = HTTP_GET,
        .handler = file_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &js_uri);

    httpd_uri_t favicon_uri = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = file_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &favicon_uri);

    // API endpoints
    httpd_uri_t save_config_uri = {
        .uri = "/save_config",
        .method = HTTP_POST,
        .handler = save_config_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &save_config_uri);

    httpd_uri_t get_config_uri = {
        .uri = "/get_config",
        .method = HTTP_GET,
        .handler = get_config_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &get_config_uri);

    httpd_uri_t get_metric_uri = {
        .uri = "/get_metric",
        .method = HTTP_GET,
        .handler = get_metric_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &get_metric_uri);

    httpd_uri_t get_version_info_uri = {
        .uri = "/get_version_info",
        .method = HTTP_GET,
        .handler = get_version_info_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &get_version_info_uri);

    // OTA API handlers (consolidated)
    httpd_uri_t ota_api_get = {
        .uri = "/api/ota",
        .method = HTTP_GET,
        .handler = ota_api_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &ota_api_get);

    httpd_uri_t ota_api_post = {
        .uri = "/api/ota",
        .method = HTTP_POST,
        .handler = ota_api_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &ota_api_post);

    // Also register HEAD handler for get_config so HEAD probes (connectivity checks) succeed
    httpd_uri_t get_config_head_uri = {
        .uri = "/get_config",
        .method = HTTP_HEAD,
        .handler = get_config_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &get_config_head_uri);

    // Also register POST handler for get_config (some clients may use POST)
    httpd_uri_t get_config_post_uri = {
        .uri = "/get_config",
        .method = HTTP_POST,
        .handler = get_config_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &get_config_post_uri);

    // Captive portal detection handlers
    httpd_uri_t hotspot_detect_uri = {
        .uri = "/hotspot-detect.html",
        .method = HTTP_GET,
        .handler = captive_portal_redirect_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &hotspot_detect_uri);

    httpd_uri_t generate_204_uri = {
        .uri = "/generate_204",
        .method = HTTP_GET,
        .handler = captive_portal_redirect_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &generate_204_uri);

    // Wildcard handler for any other requests (must be last)
    httpd_uri_t wildcard_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = captive_portal_redirect_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &wildcard_uri);
    ESP_LOGI(TAG, "Registered wildcard handler for /*");

    // Initialize OTA manager
    esp_err_t ota_ret = ota_manager_init();
    if (ota_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OTA manager: %s", esp_err_to_name(ota_ret));
    } else {
        ESP_LOGI(TAG, "OTA manager initialized successfully");
    }

    ESP_LOGI(TAG, "Web server started successfully on port %d", WEB_SERVER_PORT);
    return ESP_OK;
}

esp_err_t web_server_stop(void) {
    if (server_handle == NULL) {
        ESP_LOGW(TAG, "Web server is not running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping web server...");

    esp_err_t ret = httpd_stop(server_handle);
    if (ret == ESP_OK) {
        server_handle = NULL;
        ESP_LOGI(TAG, "Web server stopped successfully");
    } else {
        ESP_LOGE(TAG, "Failed to stop web server: %s", esp_err_to_name(ret));
    }

    return ret;
}

bool web_server_is_running(void) {
    return (server_handle != NULL);
}