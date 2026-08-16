#pragma once

#include <uefi.h>

#define MMAP_TYPE_USABLE                 1
#define MMAP_TYPE_RESERVED               2
#define MMAP_TYPE_ACPI_RECLAIMABLE       3
#define MMAP_TYPE_ACPI_NVS               4
#define MMAP_TYPE_BAD_MEM                5
#define MMAP_TYPE_BOOTLOADER_RECLAIMABLE 6

#define MMAP_FLAG_NONE                   0
#define MMAP_FLAG_UNCACHED               (1 << 0)
#define MMAP_FLAG_WRITE_COMBINING        (1 << 1)
#define MMAP_FLAG_WRITE_THROUGH          (1 << 2)
#define MMAP_FLAG_WRITE_BACK             (1 << 3)

typedef struct {
    uint32_t* address;
    uint16_t  width;
    uint16_t  height;
    uint16_t  pitch;
} __attribute__((packed)) framebuffer_t;

typedef struct {
    uint64_t phys_start;
    uint64_t length;
    uint32_t type;
    uint32_t flags;
} __attribute__((packed)) mmap_entry_t;

typedef struct {
    uint64_t total_usable;
    uint64_t max_phys_addr;
    mmap_entry_t* entries;
    uint32_t count;
} __attribute__((packed)) mmap_info_t;

typedef struct {
    uint64_t kernel_src;
    size_t kernel_size;
} __attribute__((packed)) kernel_info_t;

typedef struct {
    uint64_t addr;
    size_t size;
} __attribute__((packed)) init_info_t;

typedef struct {
    uint64_t addr;
    size_t size;
} __attribute__((packed)) initrd_info_t;

typedef struct {
    framebuffer_t framebuffer;
    mmap_info_t memory;

    kernel_info_t kernel;
    init_info_t init;
    initrd_info_t initrd;
    uint64_t rsdp_addr;
} __attribute__((packed)) boot_info_t;