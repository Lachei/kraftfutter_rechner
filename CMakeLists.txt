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

add_compile_options(-Wall)

project(dcdc-converter C CXX)

pico_sdk_init()

if (NOT PICO_CYW43_SUPPORTED)
        message(FATAL_ERROR "Pico w not supported by your sdk. Make sure to have PICO_CYW43_SUPPORTED set by your sdk installation")
endif()


# ----------------------------------------------------------------------------
# Website content
# ----------------------------------------------------------------------------
pico_add_library(dcdc-converter-hmtl NOFLAG)
pico_set_lwip_httpd_content(dcdc-converter-hmtl INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/404.html
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/index.shtml
        ${CMAKE_CURRENT_SOURCE_DIR}/http_content/test.shtml
)


# ----------------------------------------------------------------------------
# Executable
# ----------------------------------------------------------------------------
add_executable(dcdc-converter
        src/main.cpp
)
set_property(TARGET dcdc-converter PROPERTY CXX_STANDARD 23)
target_compile_definitions(dcdc-converter PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
)
target_include_directories(dcdc-converter PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PICO_LWIP_CONTRIB_PATH}/apps/httpd
)
target_link_libraries(dcdc-converter
        pico_cyw43_arch_lwip_sys_freertos
        pico_lwip_http
        pico_lwip_mdns
        pico_stdlib
        FreeRTOS-Kernel-Heap4 # FreeRTOS kernel and dynamic heap
        dcdc-converter-hmtl
)
pico_add_extra_outputs(dcdc-converter)

