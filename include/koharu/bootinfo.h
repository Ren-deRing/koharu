#pragma once

#include <stdint.h>

#define MMAP_TYPE_USABLE                 1
#define MMAP_TYPE_RESERVED               2
#define MMAP_TYPE_ACPI_NVS               3
#define MMAP_TYPE_BAD_MEM                4
#define MMAP_TYPE_BOOTLOADER_RECLAIMABLE 5
#define MMAP_TYPE_BOOTLOADER_RESERVED    6

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint32_t* fb;
} __attribute__((packed)) vbe_screen;

typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_ext_attr;
    uint64_t padding;
} __attribute__((packed)) boot_mmap_entry_t;

typedef struct {
    uint64_t total_usable;
    uint64_t max_phys_addr;
    boot_mmap_entry_t* entries;
    uint32_t count;
} __attribute__((packed)) boot_mmap_info_t;

typedef struct {
    vbe_screen screen;
    boot_mmap_info_t memory;
    uint64_t initrd_addr;
} __attribute__((packed)) boot_info_t;

extern boot_info_t* g_boot_info;