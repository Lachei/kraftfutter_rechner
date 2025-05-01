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

#define TEST_TASK_PRIORITY ( tskIDLE_PRIORITY + 1UL )

constexpr UBaseType_t STANDARD_TASK_PRIORITY = tskIDLE_PRIORITY + 1ul;
constexpr UBaseType_t CONTROL_TASK_PRIORITY = tskIDLE_PRIORITY + 10ul;

static void mdns_response_callback(struct mdns_service *service, void*)
{
  err_t res = mdns_resp_add_service_txtitem(service, "path=/", 6);
  if (res != ERR_OK)
    LogError("mdns add service txt failed");
}

void usb_comm_task(void *) {
    LogInfo("Usb communication task");
    crypto_storage::Default();

    for (;;) {
	handle_usb_command();
    }
}

void measure_task(void *) {
}

void control_task(void *) {
    get_absolute_time();

}

void wifi_search_task(void *) {
    LogInfo("Wifi task started");
    if (wifi_storage::Default().ssid_wifi.empty()) // onyl start the access point by default if no normal wifi connection is set
        access_point::Default().init();

    netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], wifi_storage::Default().hostname.data());
    mdns_resp_init();
    mdns_resp_add_netif(&cyw43_state.netif[CYW43_ITF_STA], wifi_storage::Default().hostname.data());
    mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], wifi_storage::Default().mdns_service_name.data(), "_http", DNSSD_PROTO_TCP, 80, mdns_response_callback, NULL);

    for (;;) {
        // connect to wifi
        if (wifi_storage::Default().ssid_wifi.view.size() && (CYW43_LINK_JOIN != cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) || wifi_storage::Default().wifi_changed)) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            LogInfo("Trying to connect to wifi");
            if (0 != cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 1000))
                LogWarning("failed to connect, retry in 5 seconds");
            wifi_storage::Default().wifi_changed = false;
        } 
        wifi_storage::Default().wifi_connected = CYW43_LINK_JOIN == cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, wifi_storage::Default().wifi_connected);
        // rename the hostnaem
        if (wifi_storage::Default().hostname_changed) {
            LogInfo("Hostname change detected, adopting hostname");
            netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], wifi_storage::Default().hostname.data());
            mdns_resp_rename_netif(&cyw43_state.netif[CYW43_ITF_STA], wifi_storage::Default().hostname.data());
            wifi_storage::Default().hostname_changed = false;
        }
        LogInfo("Rescan wifi access points");
        wifi_storage::Default().update_scanned();
        vTaskDelay(5000);
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
    LogInfo("Initialization done");
    std::cout << "Initialization done, get all further info via the commands shown in 'help'\n";
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    TaskHandle_t task_usb_comm;
    TaskHandle_t task_update_wifi;
    xTaskCreate(usb_comm_task, "TestMainThread", configMINIMAL_STACK_SIZE, NULL, 1, &task_usb_comm);	// usb task also has to be started only after cyw43 init as some wifi functions are available
    xTaskCreate(wifi_search_task, "UpdateWifiThread", configMINIMAL_STACK_SIZE, NULL, 1, &task_update_wifi);
    for (;;) vTaskDelay(1<<20);
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
