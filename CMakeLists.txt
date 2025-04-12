cmake_minimum_required(VERSION 3.14)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)


project(dcdc-converter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)

message("${PICO_SDK_VERSION_STRING}")

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# Initialize the SDK
pico_sdk_init()

#------------------------------------------------------------------------------------
#                           FreeRTOS library building
#------------------------------------------------------------------------------------
add_library(FreeRTOS STATIC
    ${FREERTOS_KERNEL_SRC}/event_groups.c
    ${FREERTOS_KERNEL_SRC}/list.c
    ${FREERTOS_KERNEL_SRC}/portable/GCC/ARM_CM0/port.c
    ${FREERTOS_KERNEL_SRC}/portable/MemMang/heap_3.c
    ${FREERTOS_KERNEL_SRC}/tasks.c

    )


target_include_directories(FreeRTOS PUBLIC
    include
    ${FREERTOS_KERNEL_SRC}/include
    ${FREERTOS_KERNEL_SRC}/portable/GCC/ARM_CM0
)


#------------------------------------------------------------------------------------
#                           DcDc-converter building
#------------------------------------------------------------------------------------

add_executable(dcdc-converter src/main.cpp)
target_include_directories(dcdc-converter PRIVATE include)
# cy243 is needed for the pico w board
target_link_libraries(dcdc-converter pico_stdlib pico_cyw43_arch_none FreeRTOS)

pico_add_extra_outputs(dcdc-converter)

