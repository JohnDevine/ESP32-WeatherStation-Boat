/**
 * @file version.h
 * @brief Project and component version management
 * 
 * This file provides structures and macros for version management.
 * It supports both project-level and component-level versioning.
 */

#ifndef VERSION_H
#define VERSION_H

#include <stdio.h>
#include "esp_log.h"

/**
 * @brief Structure for version information
 */
typedef struct {
    const char* component; ///< Component/file name
    const char* version;   ///< Version string
    const char* date;      ///< Date of last update
} version_info_t;

/**
 * @brief Register a component version
 * 
 * This macro creates version information that will be visible to the linker
 * and can be accessed at runtime.
 * 
 * @param component Component name (without quotes)
 * @param ver Version string (with quotes)
 * @param build_date Build date string (with quotes)
 */
#define REGISTER_VERSION(component, ver, build_date) \
    static const version_info_t component##_version_info __attribute__((used)) = { \
        #component, ver, build_date \
    }; \
    static const char component##_version_string[] __attribute__((used)) = \
        #component ":" ver ":" build_date;

// Project version from build flags (defined in platformio.ini)
#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.0.0"
#endif

#ifndef PROJECT_VERSION_MAJOR
#define PROJECT_VERSION_MAJOR 0
#endif

#ifndef PROJECT_VERSION_MINOR
#define PROJECT_VERSION_MINOR 0
#endif

#ifndef PROJECT_VERSION_PATCH
#define PROJECT_VERSION_PATCH 0
#endif

// Project name from build flags (or default if not defined)
#ifndef PROJECT_NAME
#define PROJECT_NAME "ESP32-Project"
#endif

// Project build date and time
#define PROJECT_BUILD_DATE __DATE__
#define PROJECT_BUILD_TIME __TIME__

// Register project version
REGISTER_VERSION(Project, PROJECT_VERSION, PROJECT_BUILD_DATE);

// Make project version directly accessible
static const char PROJECT_VERSION_STRING[] __attribute__((used)) = PROJECT_VERSION;

/**
 * @brief Print project version information
 * 
 * This function prints the project version and build date
 */
void print_version_info(void);

/**
 * @brief Get comprehensive version information as formatted string
 * 
 * Returns a formatted string containing all project and component version
 * information suitable for display in web interfaces. The returned string
 * is statically allocated and should not be freed.
 * 
 * @return Pointer to formatted version information string
 */
const char* get_version_info_string(void);

#endif // VERSION_H
