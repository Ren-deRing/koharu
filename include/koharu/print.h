#pragma once

typedef void (*log_putc)(char c);

void kputc(char c);
void dprintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

void set_log_putc(log_putc new_putc);