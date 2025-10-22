# ESP32 Project Versioning System

This document explains the versioning system used in this ESP32 project. The system provides both file-level and project-level version tracking that is visible to the linker and available at runtime.

## Table of Contents

1. [Overview](#overview)
2. [Project-Level Versioning](#project-level-versioning)
3. [File-Level Versioning](#file-level-versioning)
4. [Version Registry](#version-registry)
5. [Version Display Functions](#version-display-functions)
6. [Integration with Build System](#integration-with-build-system)
7. [Best Practices](#best-practices)
8. [Troubleshooting](#troubleshooting)

## Overview

The versioning system consists of:

1. **Project Version**: Overall firmware version (e.g., "2.3.1")
2. **File Versions**: Individual version for each source file or component
3. **Version Registry**: Centralized collection of all versions
4. **Version Information API**: Functions to display version information

Key features:
- All version strings are visible to the linker
- Versions can be displayed at runtime for diagnostics
- File versions only change when the specific file is modified
- Project version is managed independently of file versions

## Project-Level Versioning

### Project Version Header

The project version is defined in `include/version.h`:

```c
// include/version.h
#ifndef VERSION_H
#define VERSION_H

#include <stdio.h>
#include "esp_log.h"

// Version info structure
typedef struct {
    const char* component;
    const char* version;
    const char* date;
} version_info_t;

// Register a component version
#define REGISTER_VERSION(component, ver, build_date) \
    static const version_info_t component##_version_info __attribute__((used)) = { \
        #component, ver, build_date \
    }; \
    static const char component##_version_string[] __attribute__((used)) = \
        #component ":" ver ":" build_date;

// Project version from build flags
#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.0.0"
#endif

// Project build date
#define PROJECT_BUILD_DATE __DATE__

// Register project version
REGISTER_VERSION(Project, PROJECT_VERSION, PROJECT_BUILD_DATE);

// Function declaration
void print_version_info(void);

#endif // VERSION_H
```

### Project Version Implementation

```c
// src/version.c
#include "version.h"
#include "esp_log.h"

static const char* TAG = "VERSION";

void print_version_info(void) {
    ESP_LOGI(TAG, "Project Version: %s (Built on %s)", 
             PROJECT_VERSION, PROJECT_BUILD_DATE);
    
    // Additional component version info can be printed here
    // or by the components themselves
}
```

### Setting Project Version in platformio.ini

The project version is set in `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
build_flags = 
    -D PROJECT_VERSION_MAJOR=2
    -D PROJECT_VERSION_MINOR=3
    -D PROJECT_VERSION_PATCH=1
    -D PROJECT_VERSION=\"2.3.1\"
```

When releasing a new project version, update the version numbers in this file.

## File-Level Versioning

Each source file or logical component should include its own version information.

### Simple Approach for Single Files

For individual files, include version information directly in the file:

```c
// src/temperature_sensor.c
#include "version.h"
#include "temperature_sensor.h"

// Register component version - update when file changes
REGISTER_VERSION(TemperatureSensor, "1.2.3", "2025-08-22");

// Implementation...
void temperature_sensor_init(void) {
    ESP_LOGI("TEMP", "Initializing Temperature Sensor v1.2.3");
    // Code...
}
```

### Component Approach for Multiple Related Files

For components spanning multiple files, create a component-specific version header:

```c
// include/wifi_manager_version.h
#ifndef WIFI_MANAGER_VERSION_H
#define WIFI_MANAGER_VERSION_H

#include "version.h"

#define WIFI_MANAGER_VERSION "1.3.2"
#define WIFI_MANAGER_DATE "2025-08-22"

// Register component version
REGISTER_VERSION(WiFiManager, WIFI_MANAGER_VERSION, WIFI_MANAGER_DATE);

#endif // WIFI_MANAGER_VERSION_H
```

Then include it in the component's main file:

```c
// src/wifi_manager.c
#include "wifi_manager_version.h"
#include "wifi_manager.h"

void wifi_manager_init(void) {
    ESP_LOGI("WIFI", "Initializing WiFi Manager v" WIFI_MANAGER_VERSION);
    // Implementation...
}
```

## Version Registry

To collect all component versions in one place, create a version registry:

```c
// src/version_registry.c
#include "version.h"

// This file doesn't need to do anything else
// The REGISTER_VERSION macro in each component already
// creates the version structs with __attribute__((used))
// which ensures they are included in the binary
```

## Version Display Functions

### Simple Version Display

```c
// src/version_display.c
#include "version.h"
#include "esp_log.h"

static const char* TAG = "VERSION";

void display_all_versions(void) {
    ESP_LOGI(TAG, "----- Version Information -----");
    ESP_LOGI(TAG, "Project: v%s (Built on %s)",
        PROJECT_VERSION, PROJECT_BUILD_DATE);
    
    // Individual components can log their own versions
    // during initialization or when explicitly requested
}
```

### Example Usage in main.cpp

```cpp
### Example Usage in main.c

```c
// src/main.c
extern "C" {
    #include "version.h"
    #include "esp_log.h"
}

static const char* TAG = "MAIN";

extern "C" void app_main(void) {
    // Print project version at startup
    ESP_LOGI(TAG, "Starting ESP32-WeatherStation-2 v%s", PROJECT_VERSION);
    
    // Display all version information
    print_version_info();
    
    // Rest of application...
}
```

## Integration with Build System

### PlatformIO Build Flags

The project version is defined in build flags in `platformio.ini`. When releasing a new version, update these values:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
build_flags = 
    -D PROJECT_VERSION_MAJOR=2
    -D PROJECT_VERSION_MINOR=3
    -D PROJECT_VERSION_PATCH=1
    -D PROJECT_VERSION=\"2.3.1\"
```

### ESP-IDF Integration

If using ESP-IDF's app description feature, you can also set the version there:

```c
// src/version.c
#include "esp_app_desc.h"

// Set ESP-IDF app version to match project version
// (This requires a specific setting in sdkconfig)
#if CONFIG_APP_PROJECT_VER_FROM_CONFIG
#define APP_PROJECT_VER PROJECT_VERSION
#endif
```

## Best Practices

### When to Update Versions

1. **Project Version**: Update when making a release or significant change
2. **Component Versions**: Update when modifying a specific component
3. **File Versions**: Update when changing individual files

### Version Management Workflow

#### When updating individual components:

1. **Update the component**: Change the version string in `REGISTER_VERSION()`
2. **Update documentation**: Change `@version` comment in header file  
3. **Update date**: Use current date in both places

Example for updating a component:
```c
// In src/web_server.c
REGISTER_VERSION(WebServer, "1.0.1", "2025-10-18");

// In include/web_server.h
/**
 * @file web_server.h
 * @brief HTTP web server for captive portal interface
 * @version 1.0.1
 * @date 2025-10-18
 * @author John Devine
 * @email john.h.devine@gmail.com
 */
```

#### When releasing a new project version:

1. **Update `platformio.ini`**: Change `PROJECT_VERSION` defines
2. **Optional**: Update component versions if they changed

Example for project release:
```ini
; platformio.ini
build_flags = 
    -D PROJECT_VERSION_MAJOR=1
    -D PROJECT_VERSION_MINOR=1
    -D PROJECT_VERSION_PATCH=0
    -D PROJECT_VERSION=\"1.1.0\"
```

### Version Number Format

Use [Semantic Versioning](https://semver.org/):

- **MAJOR**: Incompatible API changes
- **MINOR**: New functionality (backwards compatible)
- **PATCH**: Bug fixes (backwards compatible)

For example: `1.2.3` where:
- `1` is the major version
- `2` is the minor version
- `3` is the patch version

### Version History Documentation

Include a brief change history in component version files:

```c
// wifi_manager_version.h
#ifndef WIFI_MANAGER_VERSION_H
#define WIFI_MANAGER_VERSION_H

#include "version.h"

/**
 * WiFi Manager Version History
 * 1.3.2 (2025-08-22): Fixed reconnection bug
 * 1.3.1 (2025-08-01): Added support for WPA3
 * 1.3.0 (2025-07-15): Added dynamic AP selection
 * 1.2.0 (2025-06-10): Initial implementation
 */
#define WIFI_MANAGER_VERSION "1.3.2"
#define WIFI_MANAGER_DATE "2025-08-22"

// Register component version
REGISTER_VERSION(WiFiManager, WIFI_MANAGER_VERSION, WIFI_MANAGER_DATE);

#endif // WIFI_MANAGER_VERSION_H
```

## Advanced Examples

### Creating a Version Command for the CLI

If your project has a command-line interface, add a version command:

```c
// src/cli_commands.c
#include "version.h"
#include "cli_engine.h"
#include "esp_log.h"

static const char* TAG = "CLI";

// CLI command to show version information
void cmd_version(int argc, char** argv) {
    ESP_LOGI(TAG, "ESP32-WeatherStation-2 v%s", PROJECT_VERSION);
    ESP_LOGI(TAG, "Build date: %s", PROJECT_BUILD_DATE);
    
    // Print component versions
    ESP_LOGI(TAG, "Components:");
    ESP_LOGI(TAG, "  WiFi Manager: v%s (%s)", WIFI_MANAGER_VERSION, WIFI_MANAGER_DATE);
    ESP_LOGI(TAG, "  Sensor Module: v%s (%s)", SENSOR_MODULE_VERSION, SENSOR_MODULE_DATE);
    ESP_LOGI(TAG, "  Display Driver: v%s (%s)", DISPLAY_DRIVER_VERSION, DISPLAY_DRIVER_DATE);
}

// Register CLI command
void register_version_command(void) {
    register_cli_command("version", "Show firmware version information", cmd_version);
}
```

### Adding Git Commit Information

For development builds, you can include Git commit information using build flags:

```ini
; platformio.ini
[env:development]
platform = espressif32
board = esp32dev
framework = espidf
extra_scripts = pre:build_scripts/get_git_info.py
build_flags = 
    -D PROJECT_VERSION=\"dev\"
    -D GIT_COMMIT=\"${GIT_COMMIT}\"
    -D GIT_BRANCH=\"${GIT_BRANCH}\"
```

Then use it in your code:

```c
// src/version.c
#include "version.h"
#include "esp_log.h"

static const char* TAG = "VERSION";

#ifdef GIT_COMMIT
#define GIT_INFO GIT_COMMIT
#else
#define GIT_INFO "unknown"
#endif

// Register git version
REGISTER_VERSION(GitCommit, GIT_INFO, PROJECT_BUILD_DATE);

void print_version_info(void) {
    ESP_LOGI(TAG, "Project Version: %s (Built on %s)", 
             PROJECT_VERSION, PROJECT_BUILD_DATE);
             
    #ifdef GIT_COMMIT
    ESP_LOGI(TAG, "Git commit: %s", GIT_COMMIT);
    #ifdef GIT_BRANCH
    ESP_LOGI(TAG, "Git branch: %s", GIT_BRANCH);
    #endif
    #endif
}
```

## Troubleshooting

### Version Symbols Not Appearing in Binary

Make sure:
1. All version declarations use `__attribute__((used))` to prevent removal
2. Version symbols are declared as `static const` variables
3. The linker is not stripping unused symbols

### Version Not Updating

1. Check that you've updated the correct version definition
2. Make sure the build system is rebuilding the necessary files
3. Clean and rebuild the project to ensure all files are recompiled

### Multiple Version Definitions

If you encounter multiple definition errors:
1. Make sure version variables are declared in header files as `extern`
2. Define the actual variables in one `.c` file only
3. Use unique names for version strings

## Example Implementation Workflow

1. **Setup Project Version**:
   - Define project version in `platformio.ini`
   - Create `version.h` and `version.c` files

2. **Add to Each Component**:
   - Add version information to each component
   - Use `REGISTER_VERSION` macro to ensure linker visibility

3. **Update When Changing**:
   - When modifying a file, update its version number
   - When releasing, update the project version

4. **Display at Runtime**:
   - Call `print_version_info()` at startup
   - Create a CLI command for on-demand version information
