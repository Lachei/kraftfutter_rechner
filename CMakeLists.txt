# ----------------------------------------------------------------------------
# General settings/includes
# ----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)
if (NOT FREERTOS_KERNEL_PATH AND NOT DEFINED ENV{FREERTOS_KERNEL_PATH})
        message(FATAL_ERROR "Missing FreeRTOS kernel source path. Set by declaring environment variable FREERTOS_KERNEL_PATH or add -DFREERTOS_KERNEL_PATH=<path> to the cmake call")
endif()

if (NOT PICO_BOARD)
        message("No board set, defaulting to pico_w")
        set(PICO_BOARD pico_w)
endif()

include(pico_sdk_import.cmake)
include(pico_extras_import_optional.cmake)
include(FreeRTOS_Kernel_import.cmake)
include(cmake/html_pages_library.cmake)

add_compile_options(-Wall)

project(dcdc-converter C CXX)

pico_sdk_init()

set(CMAKE_BUILD_TYPE "MinSizeRel")

if (NOT PICO_CYW43_SUPPORTED)
        message(FATAL_ERROR "Pico w not supported by your sdk. Make sure to have PICO_CYW43_SUPPORTED set by your sdk installation")
endif()


# ----------------------------------------------------------------------------
# Website content
# ----------------------------------------------------------------------------
add_html_pages_library(NAME dcdc-converter-html SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/404.html
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/index.html
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/style.css
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/internet.html
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/overview.html
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/settings.html
)


# ----------------------------------------------------------------------------
# Executable
# ----------------------------------------------------------------------------
add_executable(dcdc-converter
        src/main.cpp
        src/dhcpserver.c
        src/dnsserver.c
        src/log_storage.cpp
        src/ntp_client.cpp
)
set_property(TARGET dcdc-converter PROPERTY CXX_STANDARD 23)
target_compile_definitions(dcdc-converter PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
)
target_include_directories(dcdc-converter PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(dcdc-converter
        pico_stdlib
        pico_mbedtls
        hardware_flash
        pico_cyw43_arch_lwip_sys_freertos
        pico_lwip_mdns
        FreeRTOS-Kernel-Heap4 # FreeRTOS kernel and dynamic heap
        dcdc-converter-html
)
pico_add_extra_outputs(dcdc-converter)
pico_enable_stdio_usb(dcdc-converter 1)
pico_enable_stdio_uart(dcdc-converter 0)

