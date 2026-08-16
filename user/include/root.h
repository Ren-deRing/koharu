#pragma once

#include <stdint.h>

#define ROOT_BOOT_VA    0x700010001000ULL
#define ROOT_POOL_VA    0x700020000000ULL
#define ROOT_INITRD_VA  0x700030000000ULL
#define SHARED_VA       0x700040000000ULL
#define SCRATCH_VA      0x710000000000ULL

#define PAGE_SIZE 4096

#define PROT_READ   0x01
#define PROT_WRITE  0x02
#define PROT_EXEC   0x04

#define KOHARU_REQ_ALLOC 1
#define KOHARU_REQ_READY 2
#define KOHARU_REQ_MAP   3

#define CHILD_STACK_SIZE (128 * 1024)
#define CHILD_STACK_TOP  0x7ffffffff000ULL
#define CHILD_HEAP_VA    0x700000000000ULL

typedef struct {
    uint32_t *address;
    uint16_t  width;
    uint16_t  height;
    uint16_t  pitch;
} __attribute__((packed)) fb_t;

typedef struct {
    uint64_t addr;
    uint64_t size;
} __attribute__((packed)) initrd_t;

struct root_boot {
    uint64_t  self_tid;
    fb_t      fb;
    initrd_t  initrd;
    uint64_t  pool_bytes;
    uint64_t  frame_count;
};
