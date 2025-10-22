/**
 * @file web_server.c
 * @brief HTTP web server implementation for captive portal
 * @version 0.1
 * @date 2025-10-17
 * @author John Devine
 * @email john.h.devine@gmail.com
 */

// =============================
// Includes
// =============================
#include "web_server.h"
#include "nvs_utils.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_spiffs.h"
#include "esp_log.h"

// =============================
// Function Prototypes
// =============================
static esp_err_t file_get_handler(httpd_req_t *req);
static esp_err_t save_config_handler(httpd_req_t *req);
static esp_err_t get_config_handler(httpd_req_t *req);
static esp_err_t captive_portal_redirect_handler(httpd_req_t *req);

// =============================
// Constants & Definitions
// =============================
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

static esp_err_t save_config_handler(httpd_req_t *req) {
    char content[512];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(content)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too large");
        return ESP_FAIL;
    }

    // Read POST data
    ret = httpd_req_recv(req, content, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    content[ret] = '\0';

    ESP_LOGI(TAG, "Received config data: %s", content);

    // Parse JSON-like data (simple string parsing)
    char mac_address[MAC_ADDR_STR_LEN] = "";
    char ip_address[IP_ADDR_STR_LEN] = "";
    char password[WIFI_PASS_MAX_LEN] = "";
    char active_key[33] = "";     // 32 hex chars + null terminator
    char pending_key[33] = "";

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

    // Send response
    const char *response = "{\"message\":\"Configuration saved successfully!\",\"status\":\"success\"}";
    if (save_result != ESP_OK) {
        response = "{\"message\":\"Failed to save configuration\",\"status\":\"error\"}";
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

static esp_err_t get_config_handler(httpd_req_t *req) {
    char mac_address[MAC_ADDR_STR_LEN] = "";
    char ip_address[IP_ADDR_STR_LEN] = "";
    char password[WIFI_PASS_MAX_LEN] = "";
    uint8_t active_key[ESPNOW_KEY_LEN];
    uint8_t pending_key[ESPNOW_KEY_LEN];

    // Load values from NVS
    nvs_load_server_mac(mac_address, sizeof(mac_address));
    nvs_load_ip_address(ip_address, sizeof(ip_address));
    nvs_load_wifi_password(password, sizeof(password));
    nvs_load_espnow_active_key(active_key);
    nvs_load_espnow_pending_key(pending_key);

    // Convert binary keys to hex strings
    char active_key_hex[33] = "";
    char pending_key_hex[33] = "";
    
    for (int i = 0; i < ESPNOW_KEY_LEN; i++) {
        sprintf(&active_key_hex[i*2], "%02X", active_key[i]);
        sprintf(&pending_key_hex[i*2], "%02X", pending_key[i]);
    }

    // Build JSON response
    char response[800];
    snprintf(response, sizeof(response),
             "{"
             "\"macAddress\":\"%s\","
             "\"ipAddress\":\"%s\","
             "\"password\":\"%s\","
             "\"activeKey\":\"%s\","
             "\"pendingKey\":\"%s\""
             "}",
             mac_address, ip_address, password, active_key_hex, pending_key_hex);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));

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
    config.max_uri_handlers = 16;

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