#include <stdint.h>

static const uint32_t colors[3] = { 0x301818, 0x183018, 0x181838 };

__attribute__((section("entry")))
void _start(uint64_t arg) {
    volatile uint32_t *fb = (volatile uint32_t *)0x800000;
    uint32_t color = colors[arg % 3];
    for (;;) {
        for (int i = 0; i < 2 * 1024 * 1024 / 4; i++)
            fb[i] = color;
    }
}