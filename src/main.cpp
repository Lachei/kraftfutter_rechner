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
#include "webserver.h"

#define TEST_TASK_PRIORITY ( tskIDLE_PRIORITY + 1UL )

static void mdns_response_callback(struct mdns_service *service, void*)
{
  err_t res = mdns_resp_add_service_txtitem(service, "path=/", 6);
  if (res != ERR_OK)
    LogError("mdns add service txt failed");
}

void main_task(void *) {
    if (cyw43_arch_init()) {
        LogError("failed to initialise");
        return;
    }

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    cyw43_arch_enable_sta_mode();

    const char hostname[]{"DcDcConverter"};
    netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], hostname);

    access_point ap{.name = "auf_gehts", .password="passwort"};
    ap.init();

    if (true) {
        LogInfo("Connecting to WiFi...");
        int on = 1;
        while (0 != cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 3000)) {
            LogWarning("failed to connect.");
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
            on ^= 1;
        } 
        LogInfo("Connected.");
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    mdns_resp_init();
    mdns_resp_add_netif(&cyw43_state.netif[CYW43_ITF_STA], hostname);
    mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], "lachei_tcp_server", "_http", DNSSD_PROTO_TCP, 80, mdns_response_callback, NULL);

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    Webserver().start();

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    LogInfo(std::string("Ready, running http at ") + ip4addr_ntoa(netif_ip4_addr(netif_list)));

    bool state;
    while(true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, state);
        state = !state;
        vTaskDelay(100);
    }

    mdns_resp_remove_netif(&cyw43_state.netif[CYW43_ITF_STA]);

    cyw43_arch_deinit();
}

int main( void )
{
    stdio_init_all();

    LogInfo("Starting FreeRTOS on all cores.");

    TaskHandle_t task;
    xTaskCreate(main_task, "TestMainThread", configMINIMAL_STACK_SIZE, NULL, TEST_TASK_PRIORITY, &task);

    vTaskStartScheduler();
    return 0;
}
