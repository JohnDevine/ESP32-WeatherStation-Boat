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
static esp_err_t ota_api_post_handler(httpd_req_t *req);

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

    char filepath[512];  // Increased buffer size
    const char *uri = req->uri;

    // Handle root path - serve index.html
    if (strcmp(uri, "/") == 0) {
        uri = "/index.html";
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

    // Check if file exists
    struct stat st;
    if (stat(filepath, &st) != 0) {
        ESP_LOGW(TAG, "File not found: %s", filepath);
        // Redirect to captive portal for missing files
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    // Open file
    FILE *fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
        return ESP_FAIL;
    }

    // Set content type based on file extension
    const char *mime_type = "text/plain";
    if (strstr(filepath, ".html")) {
        mime_type = "text/html";
    } else if (strstr(filepath, ".css")) {
        mime_type = "text/css";
    } else if (strstr(filepath, ".js")) {
        mime_type = "application/javascript";
    } else if (strstr(filepath, ".ico")) {
        mime_type = "image/x-icon";
    }

    httpd_resp_set_type(req, mime_type);

    // Read and send file in chunks
    char buffer[1024];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fd)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, bytes_read) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send file chunk");
            fclose(fd);
            httpd_resp_sendstr_chunk(req, NULL);  // End chunked response
            return ESP_FAIL;
        }
    }

    fclose(fd);
    httpd_resp_sendstr_chunk(req, NULL);  // End chunked response
    return ESP_OK;
}

/**
 * @brief Handle GET /api/ota
 * Query parameters:
 *  - status=1            -> return JSON status
 *  - backup=<firmware|filesystem> -> stream backup binary
 */
static esp_err_t ota_api_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "OTA GET request: %s", req->uri);

    // Check for backup request
    char query[128];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char backup_type[32];
        if (httpd_query_key_value(query, "backup", backup_type, sizeof(backup_type)) == ESP_OK) {
            ESP_LOGI(TAG, "Backup requested: %s", backup_type);
            // Stream backup partition to response
            const esp_partition_t* part = NULL;
            ota_type_t type = (strcmp(backup_type, "filesystem") == 0) ? OTA_TYPE_FILESYSTEM : OTA_TYPE_FIRMWARE;
            if (ota_get_partition_info(type, &part) != ESP_OK || part == NULL) {
                httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Backup not available");
                return ESP_FAIL;
            }

            httpd_resp_set_type(req, "application/octet-stream");
            char filename[64];
            snprintf(filename, sizeof(filename), "attachment; filename=esp32_%s_backup.bin", backup_type);
            httpd_resp_set_hdr(req, "Content-Disposition", filename);

            // Stream partition contents in chunks
            const size_t buf_size = 1024;
            uint8_t *buf = malloc(buf_size);
            if (!buf) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
                return ESP_FAIL;
            }

            size_t offset = 0;
            while (offset < part->size) {
                size_t to_read = (part->size - offset) > buf_size ? buf_size : (part->size - offset);
                esp_err_t r = esp_partition_read(part, offset, buf, to_read);
                if (r != ESP_OK) {
                    free(buf);
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Read error");
                    return ESP_FAIL;
                }
                if (httpd_resp_send_chunk(req, (const char*)buf, to_read) != ESP_OK) {
                    free(buf);
                    httpd_resp_sendstr_chunk(req, NULL);
                    return ESP_FAIL;
                }
                offset += to_read;
            }
            free(buf);
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_OK;
        }
    }

    // Default: return JSON status
    ota_status_t status;
    if (ota_get_status(&status) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get status");
        return ESP_FAIL;
    }

    char resp[512];
    snprintf(resp, sizeof(resp),
             "{\"state\":%d,\"type\":%d,\"progress\":%d,\"backup_available\":%s,\"backup_created\":%s,\"backup_skipped\":%s,\"error\":\"%s\"}",
             status.state, status.type, status.progress_percent,
             status.backup_available ? "true" : "false",
             status.backup_created ? "true" : "false",
             status.backup_skipped ? "true" : "false",
             status.error_message);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/**
 * @brief Handle POST /api/ota
 * Accepts multipart/form-data with fields: type, skipBackup, hash, file
 */
static esp_err_t ota_api_post_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "OTA POST upload received");

    // Prepare to receive multipart form data
    // Use simple streaming read and look for file content (not full RFC parser)
    int total = req->content_len;
    if (total <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty upload");
        return ESP_FAIL;
    }

    // For simplicity, read all into memory up to a capped size
    const int MAX_UPLOAD = 4 * 1024 * 1024; // 4MB
    if (total > MAX_UPLOAD) {
        httpd_resp_send_err(req, HTTPD_413_REQUEST_ENTITY_TOO_LARGE, "Upload too large");
        return ESP_FAIL;
    }

    char *buf = malloc(total);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    int received = 0;
    while (received < total) {
        int r = httpd_req_recv(req, buf + received, total - received);
        if (r <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_408_REQUEST_TIMEOUT, "Receive error");
            return ESP_FAIL;
        }
        received += r;
    }

    // Very small/naive parser: find 'Content-Disposition: form-data; name="type"' etc.
    // Extract 'type' field
    char* tptr = strstr(buf, "name=\"type\"");
    ota_config_t cfg = {0};
    cfg.create_backup = ota_get_backup_preference();
    cfg.verify_crypto = true;
    cfg.expected_hash[0] = '\0';

    if (tptr) {
        char *val = strstr(tptr, "\r\n\r\n");
        if (val) {
            val += 4;
            char *end = strstr(val, "\r\n");
            if (end) {
                char v[32];
                int len = end - val;
                if (len > 0 && len < (int)sizeof(v)) {
                    memcpy(v, val, len);
                    v[len] = '\0';
                    if (strcmp(v, "firmware") == 0) cfg.update_type = OTA_TYPE_FIRMWARE;
                    else cfg.update_type = OTA_TYPE_FILESYSTEM;
                }
            }
        }
    }

    // Check skipBackup flag
    char* skptr = strstr(buf, "name=\"skipBackup\"");
    if (skptr) {
        char *val = strstr(skptr, "\r\n\r\n");
        if (val) {
            val += 4;
            if (strncmp(val, "1", 1) == 0) cfg.create_backup = false;
        }
    }

    // Extract optional hash
    char* hp = strstr(buf, "name=\"hash\"");
    if (hp) {
        char *val = strstr(hp, "\r\n\r\n");
        if (val) {
            val += 4;
            char *end = strstr(val, "\r\n");
            if (end) {
                int len = end - val;
                if (len > 0 && len < (int)sizeof(cfg.expected_hash)) {
                    memcpy(cfg.expected_hash, val, len);
                    cfg.expected_hash[len] = '\0';
                }
            }
        }
    }

    // Find file part (first occurrence of 'filename="')
    char *fptr = strstr(buf, "filename=\"");
    if (!fptr) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No file in upload");
        return ESP_FAIL;
    }

    // Find start of file data
    char *data_start = strstr(fptr, "\r\n\r\n");
    if (!data_start) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Malformed upload");
        return ESP_FAIL;
    }
    data_start += 4;

    // Find boundary of file end (look for \r\n--)
    char *data_end = strstr(data_start, "\r\n--");
    if (!data_end) data_end = buf + total; // fallback

    size_t file_len = data_end - data_start;

    // Start OTA update with cfg
    ota_start_update(&cfg);

    // Process chunk (single chunk implementation)
    ota_set_backup_preference(cfg.create_backup);
    esp_err_t vret = ESP_OK;
    if (cfg.verify_crypto && cfg.expected_hash[0] != '\0') {
        // Convert expected hash hex string to binary
        unsigned char expected_bin[32];
        if (strlen(cfg.expected_hash) == 64) {
            for (int i = 0; i < 32; i++) {
                char byte_str[3] = { cfg.expected_hash[i*2], cfg.expected_hash[i*2+1], '\0' };
                expected_bin[i] = (unsigned char)strtoul(byte_str, NULL, 16);
            }
            if (ota_verify_crypto((const uint8_t*)data_start, file_len, expected_bin) != ESP_OK) {
                vret = ESP_FAIL;
            }
        }
    }

    if (vret == ESP_OK) {
        ota_process_chunk((const uint8_t*)data_start, file_len);
        ota_finalize_update();
    } else {
        ota_auto_rollback();
    }

    // Respond to client
    if (vret == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Verification failed");
    }

    free(buf);
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

    // Load values from NVS
    nvs_load_server_mac(mac_address, sizeof(mac_address));
    nvs_load_ip_address(ip_address, sizeof(ip_address));
    nvs_load_wifi_password(password, sizeof(password));
    nvs_load_espnow_active_key(active_key);
    nvs_load_espnow_pending_key(pending_key);
    nvs_load_boot_count(&boot_count);

    // Convert binary keys to hex strings
    char active_key_hex[33] = "";
    char pending_key_hex[33] = "";
    
    for (int i = 0; i < ESPNOW_KEY_LEN; i++) {
        sprintf(&active_key_hex[i*2], "%02X", active_key[i]);
        sprintf(&pending_key_hex[i*2], "%02X", pending_key[i]);
    }

    // Build JSON response
    char response[900];
    snprintf(response, sizeof(response),
             "{"
             "\"macAddress\":\"%s\","
             "\"ipAddress\":\"%s\","
             "\"password\":\"%s\","
             "\"activeKey\":\"%s\","
             "\"pendingKey\":\"%s\","
             "\"bootCount\":%lu"
             "}",
             mac_address, ip_address, password, active_key_hex, pending_key_hex, boot_count);

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
    ESP_LOGI(TAG, "Captive portal request: %s", req->uri);

    // Handle root requests normally
    if (strcmp(req->uri, "/") == 0) {
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

    httpd_uri_t index_html_uri = {
        .uri = "/index.html",
        .method = HTTP_GET,
        .handler = file_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &index_html_uri);

    httpd_uri_t config_html_uri = {
        .uri = "/configuration.html",
        .method = HTTP_GET,
        .handler = file_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &config_html_uri);

    httpd_uri_t information_html_uri = {
        .uri = "/information.html",
        .method = HTTP_GET,
        .handler = file_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &information_html_uri);

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