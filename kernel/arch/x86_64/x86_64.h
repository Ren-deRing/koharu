#pragma once

#include <stdint.h>

#define SERIAL_UART 0x3F8

void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);