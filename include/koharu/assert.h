#pragma once

#include <koharu/cpu.h>
#include <koharu/print.h>

#include <stdint.h>

#define assert(expr) ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__, __func__))

__attribute__((noreturn))
static inline void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    dprintf("Assertion failed: %s at %s:%d (%s)\n", assertion, file, line, function);
    
    for (;;) arch_halt();
}