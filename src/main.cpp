#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

#include <pico/stdlib.h>

void blink_led(void*) {
	for (;;) {
		gpio_put(PICO_DEFAULT_LED_PIN, true);
		sleep_ms(250);
		gpio_put(PICO_DEFAULT_LED_PIN, false);
		sleep_ms(250);
	}
}

int main () {
	TaskHandle_t task_handle_blink_led{};

	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

	for (;;) {
		gpio_put(PICO_DEFAULT_LED_PIN, true);
		sleep_ms(250);
		gpio_put(PICO_DEFAULT_LED_PIN, false);
		sleep_ms(250);
	}

	if ( pdPASS != xTaskCreate(blink_led, "taskLED", 128, NULL, 0, &task_handle_blink_led) ){
		exit(1);
	}
	vTaskStartScheduler();
	while(true){};
}
