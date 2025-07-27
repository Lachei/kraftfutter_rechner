#pragma once

#include <FreeRTOS.h>
#include "pico/stdlib.h"
#include "static_types.h"

template <int RX, int TX, int UART_ID = 0, int BAUD_RATE = 9600, int DATA_BITS = 7, int STOP_BITS = 1, uart_parity_t PARITY = UART_PARITY_EVEN> 
struct uart_connection {
	static uart_connection& Default() {
		static uart_connection uart;
		return uart;
	}

	uart_inst_t *uart{};

	uart_connection() {
		uart = UART_ID == 0 ? uart0: uart1; 
		uart_init(uart, BAUD_RATE);
		gpio_set_function(RX, UART_FUNCSEL_NUM(uart, RX));
		gpio_set_function(TX, UART_FUNCSEL_NUM(uart, TX));
		uart_set_format(uart, DATA_BITS, STOP_BITS, PARITY);
	}

	void putc(char c) { uart_putc(uart, c); }
	void puts(std::span<uint8_t> bytes) { for (uint8_t b: bytes) uart_putc(uart, b); }
	void puts(std::string_view bytes) { for (char b: bytes) uart_putc(uart, b); }
	char getc() { return uart_getc(uart); }
};

using uart_futterstationen = uart_connection<17, 16, 0, 1200>;

