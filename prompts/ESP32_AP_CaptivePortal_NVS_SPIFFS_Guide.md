# ESP32 Access Point + Captive Portal + NVS + SPIFFS Setup Guide (ESP-IDF)

This document outlines the complete process for creating an ESP32-based Access Point (AP) that serves web pages via a captive portal, with persistent storage using NVS and optional file storage via SPIFFS. The flow supports a configuration portal accessible from an iPad or any Wi-Fi client.

---

## 1. Complete Setup Flow

### **Wi-Fi Access Point + Captive Portal + HTTP Server + NVS + SPIFFS**

1. **Initialize NVS (namespace = `config`)**
   - Used for storing persistent configuration (e.g., keys, addresses).

2. **Initialize TCP/IP stack**
   - Sets up lwIP for network communication.

3. **Create default Wi-Fi AP network interface**
   - Use `esp_netif_create_default_wifi_ap()` to create the AP interface.

4. **Initialize Wi-Fi driver**
   - Configure Wi-Fi driver with `esp_wifi_init()`.

5. **Configure AP settings**
   - Define SSID, password, and maximum number of clients using `wifi_config_t`.

6. **Start Wi-Fi driver**
   - Call `esp_wifi_start()` and verify AP startup.

7. **Initialize SPIFFS (optional but recommended)**
   - Mount the SPI Flash File System for serving static web files (HTML, CSS, JS).

8. **Start HTTP server**
   - Use `esp_http_server.h` APIs to register and serve URIs.

9. **Register URI handlers for each page**
   - `/`, `/config`, `/status`, `/about`, etc.

10. **Serve pages and process HTTP requests**
    - Serve HTML pages (GET) and handle form submissions (POST).

11. **NVS storage and retrieval**
    - Store and retrieve key-value pairs (e.g., MAC, encryption keys).

12. **Handle errors and provide user feedback**
    - Display messages on web pages or via logs.

---

## 2. Captive Portal Flow

1. **Client connects to ESP32 AP**
2. **Client receives DHCP IP (ESP32 is gateway)**
3. **Client requests any HTTP page**
4. **DNS server running on ESP32 redirects all domains to ESP32 IP**
5. **HTTP server intercepts unknown requests and redirects to main portal**
6. **Serve the portal page (main configuration UI)**
7. **User updates stored values and submits forms**
8. **ESP32 stores new values to NVS**
9. **Optionally redirect to another page (confirmation or status)**

---

## 3. SPIFFS Setup

1. **Include SPIFFS header**
   ```c
   #include "esp_spiffs.h"
   ```

2. **Initialize SPIFFS**
   ```c
   esp_vfs_spiffs_conf_t conf = {
       .base_path = "/spiffs",
       .partition_label = NULL,
       .max_files = 5,
       .format_if_mount_failed = true
   };

   ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
   ```

3. **Verify mount**
   ```c
   size_t total = 0, used = 0;
   ESP_ERROR_CHECK(esp_spiffs_info(NULL, &total, &used));
   ESP_LOGI("SPIFFS", "Partition size: total: %d, used: %d", total, used);
   ```

4. **Store HTML files in `/data` directory**
   - Example: `main.html`, `config.html`, `style.css`, `script.js`.

5. **Serve static files**
   - Use `httpd_resp_sendstr_from_file()` or custom file handler for `/spiffs/...`.

---

## 4. NVS Data Management

### Namespace
All key-value pairs are stored under the namespace:
```c
"config"
```

### Keys and Data Types

| Key Name         | Description                   | Data Type | Example Value                  |
|------------------|--------------------------------|------------|--------------------------------|
| `server_mac`     | MAC address of paired server   | String     | "AA:BB:CC:DD:EE:FF"            |
| `espnow_enc_key` | Primary ESP-NOW encryption key | Binary (16 bytes) | 0x12, 0x34, 0x56, ...   |
| `espnow_rot_key` | Rotating ESP-NOW key           | Binary (16 bytes) | 0x9A, 0xBC, 0xDE, ...   |

### Initialization
```c
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
}
```

### Writing Values
```c
nvs_handle_t nvs_handle;
ESP_ERROR_CHECK(nvs_open("config", NVS_READWRITE, &nvs_handle));

// Save Server MAC
const char *mac_str = "AA:BB:CC:DD:EE:FF";
ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "server_mac", mac_str));

// Save ESP-NOW Encryption Key
uint8_t enc_key[16] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
                        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "espnow_enc_key", enc_key, sizeof(enc_key)));

// Save ESP-NOW Rotate Key
uint8_t rot_key[16] = { 0x9A, 0xBC, 0xDE, 0xF1, 0x23, 0x45, 0x67, 0x89,
                        0xAB, 0xCD, 0xEF, 0x10, 0x32, 0x54, 0x76, 0x98 };
ESP_ERROR_CHECK(nvs_set_blob(nvs_handle, "espnow_rot_key", rot_key, sizeof(rot_key)));

ESP_ERROR_CHECK(nvs_commit(nvs_handle));
nvs_close(nvs_handle);
```

### Reading Values
```c
char mac_buf[18];
size_t len = sizeof(mac_buf);
ESP_ERROR_CHECK(nvs_get_str(nvs_handle, "server_mac", mac_buf, &len));
printf("Server MAC: %s\n", mac_buf);

uint8_t enc_key[16];
size_t key_len = sizeof(enc_key);
ESP_ERROR_CHECK(nvs_get_blob(nvs_handle, "espnow_enc_key", enc_key, &key_len));

uint8_t rot_key[16];
size_t rot_len = sizeof(rot_key);
ESP_ERROR_CHECK(nvs_get_blob(nvs_handle, "espnow_rot_key", rot_key, &rot_len));
```

### Recommended Utility Module
```
main/
 ├── nvs_utils.c
 ├── nvs_utils.h
```
**nvs_utils.h:**
```c
void nvs_store_server_mac(const char *mac);
void nvs_load_server_mac(char *mac, size_t len);
void nvs_store_enc_key(const uint8_t *key);
void nvs_load_enc_key(uint8_t *key);
void nvs_store_rot_key(const uint8_t *key);
void nvs_load_rot_key(uint8_t *key);
```

---

## 5. Overall Process (Summary)

1. Initialize NVS and SPIFFS
2. Start Wi-Fi AP
3. Start captive DNS server redirecting all traffic
4. Launch HTTP server and register URI handlers
5. Serve pages and handle form interactions
6. Read/write persistent settings in NVS
7. Provide confirmation or error messages via UI
8. Optionally redirect after configuration

---

## 6. Mermaid Flow Diagram

```mermaid
flowchart TD
    A[Device Boot] --> B[Initialize NVS]
    B --> C[Initialize SPIFFS]
    C --> D[Initialize TCP/IP Stack]
    D --> E[Create Wi-Fi AP Interface]
    E --> F[Configure AP SSID & Password]
    F --> G[Start Wi-Fi Driver]
    G --> H[Run Captive DNS Redirect]
    H --> I[Start HTTP Server]
    I --> J[Serve Portal Page]
    J --> K[User Edits Configuration Fields]
    K --> L[Submit Form (POST)]
    L --> M[Save to NVS]
    M --> N{Save Success?}
    N -->|Yes| O[Reload or Redirect to Confirmation Page]
    N -->|No| P[Display Error and Retry]
    O --> Q[Wait for Next Connection]
    P --> Q
```
