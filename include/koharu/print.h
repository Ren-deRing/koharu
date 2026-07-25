#pragma once

typedef void (*log_putc)(char c);

void kputc(char c);
void dprintf(const char* fmt, ...);

void set_log_putc(log_putc new_putc);