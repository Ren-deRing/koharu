#pragma once

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