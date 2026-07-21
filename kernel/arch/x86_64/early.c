#include <koharu/initcall.h>

#include "x86_64.h"

#include <stdint.h>

void uart_putc(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0); // waiting for ready....
    
    outb(0x3F8, c); // write!
}

int uart_init() {
    outb(SERIAL_UART + 1, 0x00); // no interrupt mode (polled mode)
    outb(SERIAL_UART + 3, 0x80); // DLAB on (speed setting mode)
    outb(SERIAL_UART + 0, 0x01); // baud rate 115200 (Low)
    outb(SERIAL_UART + 1, 0x00); // baud rate 115200 (High)
    outb(SERIAL_UART + 3, 0x03); // DLAB Off + 8 bits of text, no parity, 1 stop bit
    outb(SERIAL_UART + 2, 0xC7); // FIFO enable, clear FIFO, 14-byte threshold
    outb(SERIAL_UART + 4, 0x0B); // IRQs enabled, RTS/DSR set (ready to send)

    uart_putc('R');
    return 0;
}

early_initcall(uart_init, 0);