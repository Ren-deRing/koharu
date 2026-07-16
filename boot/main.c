#include <stdint.h>
#include <stdarg.h>

#include <string.h>

#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"

int x = 0;
int y = 0;

void dprintf(char const * fmt, ...) {
    volatile uint16_t* video_mem = (volatile uint16_t *)0xB8000; // VGA text buffer
    uint16_t color = 0x0F; // black bg & white text

    char buffer[256]; // i don't care about 256+
    va_list ap; // variable parameters
    va_start(ap, fmt); // get parameters!

    npf_vsnprintf(buffer, sizeof(buffer), fmt, ap); // help me, nanoprintf!

    va_end(ap); // no longer needed.

    // offset = ( y * 80 + x ) * 2, because max_x = 80
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == '\n') { // Newline
            x = 0; // CF
            y++;   // LF
        } else {
            video_mem[y * 80 + x] = (color << 8) | (uint16_t)buffer[i];
            x++; // VGA text mode basically reads character data from the VGA text buffer and displays.
            
            if (x >= 80) { // no off-screen
                x = 0;
                y++;
            }
        }

        if (y >= 25) {
            memmove((void *)video_mem, (const void *)(video_mem + 80), (80 * 2 * 24)); // shift the text data up!
            memset((void *)(video_mem + (80 * 24)), 0, (80 * 2)); // clear last line
            y = 24;
        }
    }
}

void loader_entry() {
    memset((void *)0xB8000, 0, (80 * 2 * 25));
    dprintf("Hello from Bootloader!\n");

    extern void init_hhdm(void);
    init_hhdm();

    dprintf("After HHDM!\n");

    for (;;);
}