/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/ip4_addr.h"
#include "lwip/apps/mdns.h"

#include "FreeRTOS.h"
#include "task.h"

#include "log_storage.h"
#include "access_point.h"
#include "wifi_storage.h"
#include "webserver.h"
#include "usb_interface.h"
#include "settings.h"
#include "measurements.h"
#include "crypto_storage.h"
#include "kuhspeicher.h"
#include "ntp_client.h"
#include "uart_storage.h"

#define TEST_TASK_PRIORITY ( tskIDLE_PRIORITY + 1UL )

constexpr UBaseType_t STANDARD_TASK_PRIORITY = tskIDLE_PRIORITY + 1ul;
constexpr UBaseType_t CONTROL_TASK_PRIORITY = tskIDLE_PRIORITY + 10ul;

void usb_comm_task(void *) {
    LogInfo("Usb communication task");
    crypto_storage::Default();

    for (;;) {
	handle_usb_command();
    }
}

void recieve_task(void *) {
    static_vector<char, 6, uint8_t> data{};
    LogInfo("startin recieve task");
    for (;;) {
        while(data.size() < data.storage.size())
            data.push(uart_futterstationen::Default().getc());
        data.clear();
        LogInfo("Uart Package recieved: 0x{:x} 0x{:x} 0x{:x} 0x{:x} 0x{:x} 0x{:x}",
                data[0], data[1], data[2], data[3], data[4], data[5]);
    }
}

void transmit_task(void *) {
    std::array<uint8_t, 4> pack_a{0x41, 0xd2, 0x84, 0x56};
    std::array<uint8_t, 4> pack_b{0xc0, 0xd2, 0x84, 0x56};
    for (;;) {
        uart_futterstationen::Default().puts(pack_a);
        vTaskDelay(500);
        uart_futterstationen::Default().puts(pack_b);
        vTaskDelay(500);
    }
}

void wifi_search_task(void *) {
    LogInfo("Wifi task started");
    if (wifi_storage::Default().ssid_wifi.empty()) // only start the access point by default if no normal wifi connection is set
        access_point::Default().init();

    wifi_storage::Default().update_hostname();
    int wifi_disconnected_count{};

    for (;;) {
        LogInfo("Wifi update loop");
        wifi_storage::Default().check_set_reboot();
        wifi_storage::Default().update_wifi_connection();
        if (wifi_storage::Default().wifi_connected)
            wifi_disconnected_count = 0;
        else
            wifi_disconnected_count += 1;
        if (wifi_disconnected_count >= 5)
            access_point::Default().init();

        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, wifi_storage::Default().wifi_connected);
        wifi_storage::Default().update_hostname();
        wifi_storage::Default().update_scanned();
        if (wifi_storage::Default().wifi_connected)
            ntp_client::Default().update_time();
        vTaskDelay(wifi_storage::Default().wifi_connected ? 5000: 1000);
    }
}


// task to initailize everything and only after initialization startin all other threads
// cyw43 init has to be done in freertos task because it utilizes freertos synchronization variables
void startup_task(void *) {
    LogInfo("Starting initialization");
    std::cout << "Starting initialization\n";
    if (cyw43_arch_init()) {
        for (;;) {
            vTaskDelay(1000);
            LogError("failed to initialize\n");
            std::cout << "failed to initialize arch (probably ram problem, increase ram size)\n";
        }
    }
    cyw43_arch_enable_sta_mode();
    Webserver().start();
    LogInfo("Ready, running http at {}", ip4addr_ntoa(netif_ip4_addr(netif_list)));
    LogInfo("Loaded cow storage with {} cows", kuhspeicher::Default().cows_size());
    LogInfo("Sanitizing cows...");
    kuhspeicher::Default().sanitize_cows();
    LogInfo("Sanitizing cows done");
    LogInfo("Loading settings");
    persistent_storage_t::Default().read(&persistent_storage_layout::setting, settings::Default());
    if (settings::Default().sanitize())
        persistent_storage_t::Default().write(settings::Default(), &persistent_storage_layout::setting);
    LogInfo("Loading settings done");
    LogInfo("Loading last feeds");
    kuhspeicher::Default().reload_last_feeds();
    LogInfo("Loading last feeds done");
    LogInfo("Initialization done");
    // singleton initiliazation ()
    uart_futterstationen::Default();
    static_format<128>("");
    static_format<8>("");
    std::cout << "Initialization done, get all further info via the commands shown in 'help'\n";
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    TaskHandle_t task_usb_comm{};
    TaskHandle_t task_update_wifi{};
    TaskHandle_t task_transmit_task{};
    auto err = xTaskCreate(usb_comm_task, "usb_comm", configMINIMAL_STACK_SIZE / 4, NULL, 1, &task_usb_comm);	// usb task also has to be started only after cyw43 init as some wifi functions are available
    if (err != pdPASS)
        LogError("Failed to start usb communication task with code {}" ,err);
    err = xTaskCreate(wifi_search_task, "UpdateWifiThread", configMINIMAL_STACK_SIZE / 4, NULL, 1, &task_update_wifi);
    if (err != pdPASS)
        LogError("Failed to start usb communication task with code {}" ,err);
    err = xTaskCreate(transmit_task, "UartTransmitTask", 128, NULL, 1, &task_transmit_task);
    if (err != pdPASS)
        LogError("Failed to start uart communication task with code {}" ,err);

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    recieve_task(nullptr);
}

int main( void )
{
    stdio_init_all();

    LogInfo("Starting FreeRTOS on all cores.");
    std::cout << "Starting FreeRTOS on all cores\n";

    TaskHandle_t task_startup;
    xTaskCreate(startup_task, "StartupThread", configMINIMAL_STACK_SIZE, NULL, 1, &task_startup);

    vTaskStartScheduler();
    return 0;
}
