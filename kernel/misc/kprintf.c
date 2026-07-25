#include <koharu/print.h>

#include <stdarg.h>

#define NANOPRINTF_IMPLEMENTATION
#include "../lib/nanoprintf.h"

static log_putc klog_putc;

void set_log_putc(log_putc new_putc) {
    klog_putc = new_putc;
}

void kputc(char c) {
    if (!klog_putc) return;
    klog_putc(c);
}

void dprintf(const char* fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);

    npf_vsnprintf(buffer, sizeof(buffer), fmt, args);

    va_end(args);

    for (int i = 0; buffer[i] != '\0'; i++) {
        kputc(buffer[i]);
    }
}