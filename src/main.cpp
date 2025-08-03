/**
 * Copyright (c) 2025 Josef Stumpfegger
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
#include "kraftfutterstation.h"

void usb_comm_task(void *) {
    LogInfo("Usb communication task");
    crypto_storage::Default();

    for (;;) {
	handle_usb_command();
    }
}

void recieve_task(void *) {
    LogInfo("starting recieve task");
    kraftfutterstation<>::Default().receive_packages();
}

void kraftfutter_send_task(void *) {
    LogInfo("Starting kraftfutter communcation task");
    for (;;) {
        int delay = kraftfutterstation<>::Default().handle_station_communication();
        if (delay)
            vTaskDelay(delay);
    }
}

void check_problematic_cows_task(void *) {
    LogInfo("Starting p1oblematic cows task");
    for (;;) {
        LogInfo("Updating problematic cows");
        // if not yet time synchronized rerun earlier
        if (ntp_client::Default().ntp_time == 0) {
            vTaskDelay(1000);
            continue;
        }
        kuhspeicher::Default().check_for_problematic_cows();
        vTaskDelay(5000);
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
    // singleton initiliazations...
    uart_futterstationen::Default();
    kraftfutterstation<>::Default();
    static_format<128>("");
    static_format<8>("");
    std::cout << "Initialization done, get all further info via the commands shown in 'help'\n";
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    TaskHandle_t task_usb_comm{};
    TaskHandle_t task_update_wifi{};
    TaskHandle_t task_recieve{};
    TaskHandle_t task_problematic_cows{};
    auto err = xTaskCreate(usb_comm_task, "usb_comm", 512, NULL, 0, &task_usb_comm);	// usb task also has to be started only after cyw43 init as some wifi functions are available
    if (err != pdPASS)
        LogError("Failed to start usb communication task with code {}" ,err);
    err = xTaskCreate(wifi_search_task, "UpdateWifiThread", 512, NULL, 0, &task_update_wifi);
    if (err != pdPASS)
        LogError("Failed to start usb communication task with code {}" ,err);
    err = xTaskCreate(recieve_task, "UartRecTask", 512, NULL, 0, &task_recieve);
    if (err != pdPASS)
        LogError("Failed to start uart recieve task 1 with code {}" ,err);
    err = xTaskCreate(check_problematic_cows_task, "ProbCows", 512, NULL, 0, &task_problematic_cows);
    if (err != pdPASS)
        LogError("Failed to start problematic cows task with code {}" ,err);

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    kraftfutter_send_task(nullptr);
}

int main( void )
{
    stdio_init_all();

    LogInfo("Starting FreeRTOS on all cores.");
    std::cout << "Starting FreeRTOS on all cores\n";

    TaskHandle_t task_startup;
    xTaskCreate(startup_task, "StartupThread", 512, NULL, 0, &task_startup);

    vTaskStartScheduler();
    return 0;
}
