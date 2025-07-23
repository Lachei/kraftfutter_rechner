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

void emit_package(std::string_view src, const static_vector<char, 16, uint8_t> &data) {
    static_string<32, uint8_t> output{};
    for (char c: data) {
        if (std::isalpha(c) || std::isdigit(c))
            output.append(c), output.append(' ');
        else
            output.append_formatted("0x{:x} ", c);
    }
    output.make_c_str_safe();
    LogError("{},{}: {}", time_us_64() / 1000, src, output.data());
}

struct uart_frame_state {
    uint64_t time;
    static_vector<char, 16, uint8_t> package{};
};
template <typename uart>
static uart_frame_state& uart_state() {
    static uart_frame_state state{};
    return state;
}

template <typename uart>
void recieve_task(void *) {
    LogInfo("startin recieve task");
    for (;;) {
        uart_state<uart>().package.push(uart::Default().getc());
        uart_state<uart>().time = time_us_64();
    }
}

void monitor_task(void *) {
    LogInfo("Starting monitor task");
    constexpr uint64_t TIMEOUT{24000}; // 8 ms
    auto & s1 = uart_state<uart_futterstationen>();
    auto & s2 = uart_state<uart_f2>();
    for (;;) {
        if (time_us_64() - s1.time > TIMEOUT && s1.package.size()) {
            emit_package("1", s1.package);
            s1.package.clear();
        }
        if (time_us_64() - s2.time > TIMEOUT && s2.package.size()) {
            emit_package("2", s2.package);
            s2.package.clear();
        }
        vTaskDelay(20);
    }
}

void transmit_task(void *) {
    std::array<uint8_t, 4> pack_a{'A', 'R', 0x04, 'V'};
    std::array<uint8_t, 4> pack_b{0x11, '1', 0x04, '5'};
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
    uart_state<uart_futterstationen>();
    uart_state<uart_f2>();
    std::cout << "Initialization done, get all further info via the commands shown in 'help'\n";
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    TaskHandle_t task_usb_comm{};
    TaskHandle_t task_update_wifi{};
    TaskHandle_t task_transmit_task{};
    TaskHandle_t task_recieve_1{};
    TaskHandle_t task_recieve_2{};
    auto err = xTaskCreate(usb_comm_task, "usb_comm", 512, NULL, 0, &task_usb_comm);	// usb task also has to be started only after cyw43 init as some wifi functions are available
    if (err != pdPASS)
        LogError("Failed to start usb communication task with code {}" ,err);
    err = xTaskCreate(wifi_search_task, "UpdateWifiThread", 512, NULL, 0, &task_update_wifi);
    if (err != pdPASS)
        LogError("Failed to start usb communication task with code {}" ,err);
    err = xTaskCreate(transmit_task, "UartTransmitTask", 128, NULL, 0, &task_transmit_task);
    if (err != pdPASS)
        LogError("Failed to start uart communication task with code {}" ,err);
    err = xTaskCreate(recieve_task<uart_futterstationen>, "UartRecTask1", 128, NULL, 0, &task_recieve_1);
    if (err != pdPASS)
        LogError("Failed to start uart recieve task 1 with code {}" ,err);
    err = xTaskCreate(recieve_task<uart_f2>, "UartRecTask2", 128, NULL, 0, &task_recieve_2);
    if (err != pdPASS)
        LogError("Failed to start uart recieve task 2 with code {}" ,err);

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    monitor_task(nullptr);
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
