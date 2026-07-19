#include <stdint.h>
#include <stdarg.h>

#include <string.h>

#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"

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
    uint16_t screen_pitch    = *(uint16_t *)(vbe_base + 16);

    uint64_t vbe_lfb_end = (uint64_t)vbe_lfb_addr + ((uint64_t)screen_pitch * screen_height);

    extern void init_hhdm(uint64_t mem_size, uint64_t vbe_lfb_end);
    init_hhdm(get_total_ram(), vbe_lfb_end);

    volatile uint32_t *fb = (volatile uint32_t *)(0xFFFF800000000000ULL + vbe_lfb_addr);
    uint32_t total_pixels  = (uint32_t)screen_width * screen_height;

    for (uint32_t i = 0; i < total_pixels; i++) {
        fb[i] = 0x00A8FF; 
    }

    for (;;) asm __volatile__ ("hlt");
}