#include <koharu/initcall.h>

#include "x86_64.h"

#include <koharu/print.h>
#include <stdint.h>
#include <stdbool.h>

bool initialized = false;

void uart_putc(char c) {
    if (!initialized) return;
    while ((inb(SERIAL_UART + 5) & 0x20) == 0); // waiting for ready....
    
    outb(0x3F8, c); // write!
}

int uart_init() {
    if (initialized) return 0; // already initialized, skip.

    outb(SERIAL_UART + 1, 0x00); // no interrupt mode (polled mode)
    outb(SERIAL_UART + 3, 0x80); // DLAB on (speed setting mode)
    outb(SERIAL_UART + 0, 0x01); // baud rate 115200 (Low)
    outb(SERIAL_UART + 1, 0x00); // baud rate 115200 (High)
    outb(SERIAL_UART + 3, 0x03); // DLAB Off + 8 bits of text, no parity, 1 stop bit
    outb(SERIAL_UART + 2, 0xC7); // FIFO enable, clear FIFO, 14-byte threshold
    outb(SERIAL_UART + 4, 0x0B); // IRQs enabled, RTS/DSR set (ready to send)

    initialized = true;
    return 0;
}

int early_log_init() {
    set_log_putc(uart_putc);
    return 0;
}

early_initcall(uart_init, 0);
early_initcall(early_log_init, 1);