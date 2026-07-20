#include <stdint.h>

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint32_t* fb;
} __attribute__((packed)) vbe_screen;

typedef struct {
    uint64_t total_usable;
    uint64_t max_phys_addr;
    uint64_t entries_addr;
    uint32_t count;
} __attribute__((packed)) boot_mmap_info_t;

typedef struct {
    vbe_screen screen;
    boot_mmap_info_t memory;
    uint64_t initrd_addr;
} __attribute__((packed)) boot_info_t;

void generic_entry(boot_info_t* boot_info) {
    volatile uint32_t* fb = boot_info->screen.fb;
    uint32_t total_pixels = (boot_info->screen.pitch / 4) * boot_info->screen.height;
    
    for (uint32_t i = 0; i < total_pixels; i++) {
        fb[i] = 0xCC9BA3; // KOHARU
    }

    for (;;) asm __volatile__ ("hlt");
}