#include <koharu/compiler.h>
#include <koharu/print.h>

#include <stdarg.h>

#define NANOPRINTF_IMPLEMENTATION
#include "../lib/nanoprintf.h"

static log_putc klog_putc;

void set_log_putc(log_putc new_putc) {
    klog_putc = new_putc;
}

void kputc(char c) {
    if (klog_putc) {
        klog_putc(c);
    }
}

void _npf_putc(int c, void* ctx) {
    (void)ctx;
    kputc((char)c);
}

void dprintf(const char* fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);

    npf_vpprintf(_npf_putc, NULL, fmt, args);

    va_end(args);
}