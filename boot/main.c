#include <stdint.h>
#include <stdarg.h>

#include <string.h>

#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"

#include "font.h"

typedef struct {
    uint32_t magic;  // 0x36, 0x04, 0x03, 0x10
    uint8_t  mode;
    uint8_t  char_height;
} __attribute__((packed)) psf_header_t;

volatile uint32_t *fb;
uint16_t screen_pitch;

void putc(char c, int x, int y, uint32_t fg) { // prototype. will be fixed.
    uintptr_t font_addr = (0xFFFF800000000000ULL + (uintptr_t)font);
    
    psf_header_t *header = (psf_header_t *)font_addr;
    uint8_t *glyph = (uint8_t *)font_addr + 4 + (c * 16);

    for (int cy = 0; cy < 16; cy++) {
        uint8_t line = glyph[cy];
        for (int cx = 0; cx < 8; cx++) {
            if (line & (0x80 >> cx)) {
                uint32_t fb_index = (y + cy) * (screen_pitch / 4) + (x + cx);
                fb[fb_index] = fg;
            }
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

    uint16_t screen_width  = (int)*(uint16_t *)(vbe_base + 18);
    uint16_t screen_height = (int)*(uint16_t *)(vbe_base + 20);
    screen_pitch = *(uint16_t *)(vbe_base + 16);

    uint64_t vbe_lfb_end = (uint64_t)vbe_lfb_addr + ((uint64_t)screen_pitch * screen_height);

    extern void init_hhdm(uint64_t mem_size, uint64_t vbe_lfb_end);
    init_hhdm(get_total_ram(), vbe_lfb_end);

    fb = (volatile uint32_t *)(0xFFFF800000000000ULL + vbe_lfb_addr);
    uint32_t total_pixels  = (uint32_t)screen_width * screen_height;

    putc('k', 100, 100, 0xFFFFFFFF);
    putc('i', 110, 100, 0xFFFFFFFF);
    putc('n', 120, 100, 0xFFFFFFFF);
    putc('i', 130, 100, 0xFFFFFFFF);
    putc('t', 140, 100, 0xFFFFFFFF);
    putc('1', 150, 100, 0xFFFFFFFF);
    putc('4', 160, 100, 0xFFFFFFFF);
    putc('8', 170, 100, 0xFFFFFFFF);

    for (;;) asm __volatile__ ("hlt");
}