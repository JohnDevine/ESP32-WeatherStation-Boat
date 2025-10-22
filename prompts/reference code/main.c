#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG = "WebServer";

// Function prototypes
static httpd_handle_t start_webserver(void);
static esp_err_t init_spiffs(void);
esp_err_t file_get_handler(httpd_req_t *req);
esp_err_t save_config_handler(httpd_req_t *req);
esp_err_t get_config_handler(httpd_req_t *req);
void dns_server_start(void);
esp_err_t captive_portal_redirect_handler(httpd_req_t *req);

// WiFi Configuration
void wifi_init_ap(void) {
    ESP_LOGI(TAG, "Initializing WiFi AP...");

    // Initialize the underlying TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default WiFi AP
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Read password from NVS, default to "12345678"
    char wifi_password[64] = "12345678";
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("config", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t len;
        if (nvs_get_str(nvs_handle, "wifi_pass", NULL, &len) == ESP_OK && len < sizeof(wifi_password)) {
            nvs_get_str(nvs_handle, "wifi_pass", wifi_password, &len);
        }
        nvs_close(nvs_handle);
    }

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP32WebServer",
            .password = "",
            .ssid_len = strlen("ESP32WebServer"),
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };
    
    // Copy password to config
    strncpy((char *)ap_config.ap.password, wifi_password, sizeof(ap_config.ap.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP initialized with SSID: %s", ap_config.ap.ssid);
}

// Function to start the HTTP server
static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;  // Increase to accommodate all handlers

    // Start the HTTP server
    ESP_LOGI(TAG, "Starting webserver on port: %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers for serving static files
        httpd_uri_t index = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = file_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &index);

        httpd_uri_t index_html = {
            .uri       = "/index.html",
            .method    = HTTP_GET,
            .handler   = file_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &index_html);

        httpd_uri_t config_page = {
            .uri       = "/configuration.html",
            .method    = HTTP_GET,
            .handler   = file_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &config_page);

        httpd_uri_t styles_css = {
            .uri       = "/styles.css",
            .method    = HTTP_GET,
            .handler   = file_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &styles_css);

        httpd_uri_t scripts_js = {
            .uri       = "/scripts.js",
            .method    = HTTP_GET,
            .handler   = file_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &scripts_js);

        httpd_uri_t favicon_ico = {
            .uri       = "/favicon.ico",
            .method    = HTTP_GET,
            .handler   = file_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &favicon_ico);

        httpd_uri_t save_config = {
            .uri       = "/save_config",
            .method    = HTTP_POST,
            .handler   = save_config_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &save_config);

        httpd_uri_t get_config = {
            .uri       = "/get_config",
            .method    = HTTP_GET,
            .handler   = get_config_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &get_config);

        // Specific captive portal detection handlers
        httpd_uri_t hotspot_detect_uri = {
            .uri       = "/hotspot-detect.html",
            .method    = HTTP_GET,
            .handler   = captive_portal_redirect_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &hotspot_detect_uri);

        httpd_uri_t generate_204_uri = {
            .uri       = "/generate_204",
            .method    = HTTP_GET,
            .handler   = captive_portal_redirect_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &generate_204_uri);

        // Wildcard handler for any other requests
        httpd_uri_t captive_uri = {
            .uri       = "/*",
            .method    = HTTP_GET,
            .handler   = captive_portal_redirect_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &captive_uri);
    }

    return server;
}

// Function to initialize SPIFFS
static esp_err_t init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/data",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    // Initialize SPIFFS
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret == ESP_FAIL) {
        // Mount failed, format the partition and try again
        ret = esp_spiffs_format(NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to format SPIFFS (%s)", esp_err_to_name(ret));
            return ret;
        }
        ret = esp_vfs_spiffs_register(&conf);
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Partition size: total=%zu, used=%zu", total, used);

    return ESP_OK;
}

// Function to handle file requests
esp_err_t file_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "HTTP request received: %s %s", req->method == HTTP_GET ? "GET" : "OTHER", req->uri);

    char filepath[1024];
    
    // Handle root path - serve index.html
    const char *uri = req->uri;
    if (strcmp(uri, "/") == 0) {
        uri = "/index.html";
    }
    
    if (strlen(uri) + 6 >= sizeof(filepath)) {  // 6 = length of "/data" + null terminator
        ESP_LOGW(TAG, "URI too long: %s", uri);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, NULL);
        return ESP_FAIL;
    }
    strcpy(filepath, "/data");
    strcat(filepath, uri);

    FILE *fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGW(TAG, "File not found: %s, redirecting to captive portal", filepath);
        // File not found - redirect to captive portal
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    fseek(fd, 0L, SEEK_END);
    size_t filesize = ftell(fd);
    fseek(fd, 0L, SEEK_SET);

    char *buf = (char *)malloc(filesize + 1);
    if (!buf) {
        fclose(fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
        return ESP_FAIL;
    }

    size_t bytes_read = fread(buf, 1, filesize, fd);
    fclose(fd);

    if (bytes_read != filesize) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
        return ESP_FAIL;
    }

    buf[filesize] = '\0';

    const char *mime_type;
    if (strstr(filepath, ".html")) mime_type = "text/html";
    else if (strstr(filepath, ".css")) mime_type = "text/css";
    else if (strstr(filepath, ".js")) mime_type = "application/javascript";
    else mime_type = "application/octet-stream";

    httpd_resp_set_type(req, mime_type);
    httpd_resp_send(req, buf, filesize);

    free(buf);

    return ESP_OK;
}

// Function to handle configuration save requests
esp_err_t save_config_handler(httpd_req_t *req) {
    char buf[512];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too large");
        return ESP_FAIL;
    }

    // Read the POST data
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read request");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "Received config data: %s", buf);

    // Simple JSON parsing for {"macAddress":"value","ipAddress":"value","password":"value","activeKey":"value","pendingKey":"value"}
    char mac_address[18] = "";
    char ip_address[16] = "";
    char password[64] = "";
    char active_key[33] = "";  // ESP-NOW keys are 16 bytes (32 hex chars)
    char pending_key[33] = "";

    // Extract macAddress
    char *mac_start = strstr(buf, "\"macAddress\":\"");
    if (mac_start) {
        mac_start += 14; // Skip "macAddress":"
        char *mac_end = strchr(mac_start, '"');
        if (mac_end && (mac_end - mac_start) < sizeof(mac_address)) {
            strncpy(mac_address, mac_start, mac_end - mac_start);
            mac_address[mac_end - mac_start] = '\0';
        }
    }

    // Extract ipAddress
    char *ip_start = strstr(buf, "\"ipAddress\":\"");
    if (ip_start) {
        ip_start += 13; // Skip "ipAddress":"
        char *ip_end = strchr(ip_start, '"');
        if (ip_end && (ip_end - ip_start) < sizeof(ip_address)) {
            strncpy(ip_address, ip_start, ip_end - ip_start);
            ip_address[ip_end - ip_start] = '\0';
        }
    }

    // Extract password
    char *pass_start = strstr(buf, "\"password\":\"");
    if (pass_start) {
        pass_start += 12; // Skip "password":"
        char *pass_end = strchr(pass_start, '"');
        if (pass_end && (pass_end - pass_start) < sizeof(password)) {
            strncpy(password, pass_start, pass_end - pass_start);
            password[pass_end - pass_start] = '\0';
        }
    }

    // Extract activeKey
    char *active_start = strstr(buf, "\"activeKey\":\"");
    if (active_start) {
        active_start += 13; // Skip "activeKey":"
        char *active_end = strchr(active_start, '"');
        if (active_end && (active_end - active_start) < sizeof(active_key)) {
            strncpy(active_key, active_start, active_end - active_start);
            active_key[active_end - active_start] = '\0';
        }
    }

    // Extract pendingKey
    char *pending_start = strstr(buf, "\"pendingKey\":\"");
    if (pending_start) {
        pending_start += 14; // Skip "pendingKey":"
        char *pending_end = strchr(pending_start, '"');
        if (pending_end && (pending_end - pending_start) < sizeof(pending_key)) {
            strncpy(pending_key, pending_start, pending_end - pending_start);
            pending_key[pending_end - pending_start] = '\0';
        }
    }

    // Save to NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("config", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        if (strlen(mac_address) > 0) {
            nvs_set_str(nvs_handle, "mac_addr", mac_address);
            ESP_LOGI(TAG, "Saved MAC address: %s", mac_address);
        }
        if (strlen(ip_address) > 0) {
            nvs_set_str(nvs_handle, "ip_addr", ip_address);
            ESP_LOGI(TAG, "Saved IP address: %s", ip_address);
        }
        if (strlen(password) > 0) {
            nvs_set_str(nvs_handle, "wifi_pass", password);
            ESP_LOGI(TAG, "Saved WiFi password");
        }
        if (strlen(active_key) > 0) {
            nvs_set_str(nvs_handle, "espnow_active_key", active_key);
            ESP_LOGI(TAG, "Saved ESP-NOW active key");
        }
        if (strlen(pending_key) > 0) {
            nvs_set_str(nvs_handle, "espnow_pending_key", pending_key);
            ESP_LOGI(TAG, "Saved ESP-NOW pending key");
        }
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    }

    // Send JSON response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"message\":\"Configuration saved successfully!\"}", -1);

    return ESP_OK;
}

// Function to handle configuration retrieval requests
// Function to handle configuration retrieval requests
esp_err_t get_config_handler(httpd_req_t *req) {
    char mac_address[18] = "";
    char ip_address[16] = "";
    char password[64] = "";
    char active_key[33] = "";
    char pending_key[33] = "";

    // Load from NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("config", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t len;
        // Get MAC address
        if (nvs_get_str(nvs_handle, "mac_addr", NULL, &len) == ESP_OK && len < sizeof(mac_address)) {
            nvs_get_str(nvs_handle, "mac_addr", mac_address, &len);
        }
        // Get IP address
        if (nvs_get_str(nvs_handle, "ip_addr", NULL, &len) == ESP_OK && len < sizeof(ip_address)) {
            nvs_get_str(nvs_handle, "ip_addr", ip_address, &len);
        }
        // Get password
        if (nvs_get_str(nvs_handle, "wifi_pass", NULL, &len) == ESP_OK && len < sizeof(password)) {
            nvs_get_str(nvs_handle, "wifi_pass", password, &len);
        }
        // Get active key
        if (nvs_get_str(nvs_handle, "espnow_active_key", NULL, &len) == ESP_OK && len < sizeof(active_key)) {
            nvs_get_str(nvs_handle, "espnow_active_key", active_key, &len);
        }
        // Get pending key
        if (nvs_get_str(nvs_handle, "espnow_pending_key", NULL, &len) == ESP_OK && len < sizeof(pending_key)) {
            nvs_get_str(nvs_handle, "espnow_pending_key", pending_key, &len);
        }
        nvs_close(nvs_handle);
    }

    // Send JSON response
    char response[1024];
    snprintf(response, sizeof(response),
             "{\"macAddress\":\"%s\",\"ipAddress\":\"%s\",\"password\":\"%s\",\"activeKey\":\"%s\",\"pendingKey\":\"%s\"}",
             mac_address, ip_address, password, active_key, pending_key);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, -1);

    return ESP_OK;
}

// DNS Server Implementation
static TaskHandle_t dns_task_handle = NULL;
#define DNS_PORT 53
#define DNS_MAX_LEN 512

static void dns_server_task(void *pvParameters) {
    char rx_buffer[DNS_MAX_LEN];
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_UDP;
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DNS_PORT);
    
    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create DNS socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "DNS socket created");
    
    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "DNS socket unable to bind: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "DNS server socket bound, port %d", DNS_PORT);
    
    while (1) {
        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, 
                          (struct sockaddr *)&source_addr, &socklen);
        
        if (len < 0) {
            ESP_LOGE(TAG, "DNS recvfrom failed: errno %d", errno);
            break;
        } else {
            // Get the sender's ip address as string
            if (source_addr.ss_family == PF_INET) {
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
            }
            
            ESP_LOGI(TAG, "DNS query received from %s", addr_str);
            
            // Simple DNS response - redirect all queries to our IP
            if (len >= 12) { // Minimum DNS header size
                // Create DNS response
                uint8_t response[DNS_MAX_LEN];
                memcpy(response, rx_buffer, len); // Copy query
                
                // Set response flags
                response[2] = 0x81; // Response, no error
                response[3] = 0x80; // Recursive available
                
                // Answer count = 1
                response[6] = 0x00;
                response[7] = 0x01;
                
                // Add answer (A record pointing to our IP: 192.168.4.1)
                int pos = len;
                response[pos++] = 0xC0; // Pointer to domain name
                response[pos++] = 0x0C;
                response[pos++] = 0x00; // Type A
                response[pos++] = 0x01;
                response[pos++] = 0x00; // Class IN
                response[pos++] = 0x01;
                response[pos++] = 0x00; // TTL
                response[pos++] = 0x00;
                response[pos++] = 0x00;
                response[pos++] = 0x3C;
                response[pos++] = 0x00; // Data length
                response[pos++] = 0x04;
                response[pos++] = 192;  // IP: 192.168.4.1
                response[pos++] = 168;
                response[pos++] = 4;
                response[pos++] = 1;
                
                // Send response
                int err = sendto(sock, response, pos, 0, 
                               (struct sockaddr *)&source_addr, sizeof(source_addr));
                if (err < 0) {
                    ESP_LOGE(TAG, "Error sending DNS response: errno %d", errno);
                } else {
                    ESP_LOGI(TAG, "DNS response sent successfully");
                }
            }
        }
    }
    
    if (sock != -1) {
        ESP_LOGI(TAG, "Shutting down DNS server socket");
        shutdown(sock, 0);
        close(sock);
    }
    vTaskDelete(NULL);
}

void dns_server_start(void) {
    ESP_LOGI(TAG, "Starting DNS server task...");
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &dns_task_handle);
    if (dns_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create DNS server task");
    } else {
        ESP_LOGI(TAG, "DNS server task created successfully");
    }
}

esp_err_t captive_portal_redirect_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Captive portal request: %s", req->uri);
    
    // Handle root requests
    if (strcmp(req->uri, "/") == 0) {
        return file_get_handler(req);
    }
    
    // Common captive portal detection URLs - respond with success/redirect
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
        
        // Some devices expect a 204 response for connectivity check
        if (strstr(req->uri, "generate_204") || strstr(req->uri, "ncsi.txt")) {
            httpd_resp_set_status(req, "204 No Content");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
        
        // Others expect a redirect to captive portal
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

void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi in AP mode
    wifi_init_ap();

    // Initialize SPIFFS
    ret = init_spiffs();
    if (ret != ESP_OK) {
        return;
    }

    // Start the HTTP server
    httpd_handle_t server = start_webserver();
    if (server == NULL) {
        ESP_LOGE(TAG, "Failed to start webserver");
    }

    // Start DNS server for captive portal
    dns_server_start();
}
