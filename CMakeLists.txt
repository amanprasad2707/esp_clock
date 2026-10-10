# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

# Firmware semantic version
set(FIRMWARE_VERSION_MAJOR 1)
set(FIRMWARE_VERSION_MINOR 0)
set(FIRMWARE_VERSION_PATCH 0)

# Construct version string: 1.0.0
set(FIRMWARE_VERSION "${FIRMWARE_VERSION_MAJOR}.${FIRMWARE_VERSION_MINOR}.${FIRMWARE_VERSION_PATCH}")

# ESP-IDF application metadata version
set(PROJECT_VER "${FIRMWARE_VERSION}")

# Export firmware version macros to C/C++ source files
add_compile_definitions(
    FIRMWARE_VERSION_MAJOR=${FIRMWARE_VERSION_MAJOR}
    FIRMWARE_VERSION_MINOR=${FIRMWARE_VERSION_MINOR}
    FIRMWARE_VERSION_PATCH=${FIRMWARE_VERSION_PATCH}
    FIRMWARE_VERSION="${FIRMWARE_VERSION}"
)

# Tell ESP-IDF to scan the nested library folder for the actual component
set(EXTRA_COMPONENT_DIRS "components/u8g2")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_clock)
