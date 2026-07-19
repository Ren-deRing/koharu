#include <stdint.h>
#include <stdarg.h>

#include <string.h>

#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"

#include "font.h"

typedef struct {
    uint16_t magic;  // 0x36, 0x04, 0x03, 0x10
    uint8_t  mode;
    uint8_t  char_height;
} __attribute__((packed)) psf1_header_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t pitch; // bytes per scanline
    uint32_t* fb;
} __attribute__((packed)) vbe_screen;

vbe_screen screen;
psf1_header_t font_attribute;

void putc(char c, int x, int y, uint32_t fg) {
    uintptr_t font_addr = (0xFFFF800000000000ULL + (uintptr_t)font);
    uint8_t *glyph = (uint8_t *)font_addr + sizeof(psf1_header_t) + (c * font_attribute.char_height);

    for (int cy = 0; cy < font_attribute.char_height; cy++) {
        uint8_t line = glyph[cy];
        for (int cx = 0; cx < 8; cx++) {
            if (line & (0x80 >> cx)) {
                uint32_t fb_index = (y + cy) * (screen.pitch / 4) + (x + cx);
                screen.fb[fb_index] = fg;
            }
        }
    }
}

int x = 0;
int max_x;
int y = 0;
int max_y;

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
            putc(buffer[i], 9*x, (font_attribute.char_height+1)*y, 0xFFFFFFFF); // hard-coded wow
            x++; // VGA text mode basically reads character data from the VGA text buffer and displays.
            
            if (x >= max_x) { // no off-screen
                x = 0;
                y++;
            }
        }

        if (y >= max_y) {
            uintptr_t src_addr = (uintptr_t)screen.fb + (screen.pitch * (font_attribute.char_height + 1));
            int lastline_offset = (screen.pitch * ((max_y - 1) * (font_attribute.char_height + 1)));
            memmove((void *)screen.fb, (const void *)src_addr, lastline_offset); // shift the text data up!
            memset((void *)(uintptr_t)screen.fb + lastline_offset, 0, (screen.pitch * (font_attribute.char_height + 1))); // clear last line
            y = max_y - 1;
        }
    }
}

typedef struct {
    uint64_t base_addr;     // memory addr
    uint64_t length;        // memory area length
    uint32_t type;          // 0: Good, 1: Reserved, 2: ACPI reclaimable. 3: ACPI NVS, 4: Containing bad memory
    uint32_t acpi_ext_attr; // acpi 3.0+
    uint64_t padding;
} __attribute__((packed)) mmap_entry_t;

uint64_t get_total_ram(void) {
    volatile mmap_entry_t* mmap_array = (volatile mmap_entry_t*)(uintptr_t)0x8000;
    uint64_t addr_end = 0;

    while (mmap_array->base_addr != 0 || mmap_array->length != 0) {
        uint64_t end_addr = mmap_array->base_addr + mmap_array->length;
        
        if (end_addr > addr_end) {
            addr_end = end_addr;
        }
        mmap_array++;
    }
    return addr_end;
}

void loader_entry() {
    uint8_t *vbe_base = (uint8_t *)0x5F00;

    uint64_t vbe_lfb_addr = (uint64_t)*(uint32_t *)(vbe_base + 40);

    screen.fb = (uint32_t *)(0xFFFF800000000000ULL + vbe_lfb_addr);
    screen.width = (uint16_t)*(uint16_t *)(vbe_base + 18);
    screen.height = (uint16_t)*(uint16_t *)(vbe_base + 20);
    screen.pitch = (uint16_t)*(uint16_t *)(vbe_base + 16);

    uint64_t vbe_lfb_end = (uint64_t)vbe_lfb_addr + ((uint64_t)screen.width * screen.height);

    extern void init_hhdm(uint64_t mem_size, uint64_t vbe_lfb_end);
    init_hhdm(get_total_ram(), vbe_lfb_end);

    font_attribute.char_height = *(uint8_t *)((0xFFFF800000000000ULL + (uintptr_t)font) + 3);
    max_x = screen.width / 9; // idk this is clean... i'll fix someday... maybe?
    max_y = screen.height / (font_attribute.char_height + 1);

    dprintf("kinit148\nHello!................\n");
    dprintf("font height: %d\n", font_attribute.char_height);
    dprintf("font height: %d\n", font_attribute.char_height);
    dprintf("font height: %d\n", font_attribute.char_height);
    dprintf("font height: %d\n", font_attribute.char_height);
    dprintf("font height: %d\n", font_attribute.char_height);
    for (int i = 0; i < 262; i++) {
        dprintf("................");
    }
    dprintf("\nlast line");
    
    for (;;) asm __volatile__ ("hlt");
}