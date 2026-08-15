#include <stdint.h>

static const uint32_t colors[3] = { 0x301818, 0x183018, 0x181838 };

static inline uint64_t syscall4(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    register uint64_t r10 asm("r10") = a4;
    register uint64_t rax asm("rax") = num;
    asm volatile("syscall"
        : "+a"(rax)
        : "D"(a1), "S"(a2), "d"(a3), "r"(r10)
        : "rcx", "r11", "memory");
    return rax;
}

__attribute__((section("entry")))
void _start(uint64_t arg) {
    syscall4(0, 1, 2, 3, 4);

    volatile uint32_t *fb = (volatile uint32_t *)0x800000;
    uint32_t color = colors[arg % 3];
    for (;;) {
        for (int i = 0; i < 2 * 1024 * 1024 / 4; i++)
            fb[i] = color;
    }
}