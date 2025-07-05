#pragma once

#include <FreeRTOS.h>
#include "pico/stdlib.h"
#include "static_types.h"

template <int RX, int TX, int UART_ID = 0, int BAUD_RATE = 9600, int DATA_BITS = 8, int STOP_BITS = 1, uart_parity_t PARITY = UART_PARITY_NONE> 
struct uart_storage {
	static uart_storage& Default() {
		static uart_storage uart;
		return uart;
	}

	uart_inst_t *uart{};

	uart_storage() {
		uart = UART_ID == 0 ? uart0: uart1; 
		uart_init(uart, BAUD_RATE);
		gpio_set_function(RX, UART_FUNCSEL_NUM(uart, RX));
		gpio_set_function(TX, UART_FUNCSEL_NUM(uart, TX));
		uart_set_format(uart, DATA_BITS, STOP_BITS, PARITY);
	}

	void putc(char c) { uart_putc(uart, c); }
	void puts(std::span<uint8_t> bytes) { for (uint8_t b: bytes) uart_putc(uart, b); }
	char getc() { return uart_getc(uart); }
};

using uart_futterstationen = uart_storage<17, 16, 0, 1200>;

