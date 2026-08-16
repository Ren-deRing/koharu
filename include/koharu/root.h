#pragma once

#include <koharu/bootinfo.h>

#include <stdint.h>

#define ROOT_BOOT_VA    0x700010001000ULL
#define ROOT_POOL_VA    0x700020000000ULL
#define ROOT_INITRD_VA  0x700030000000ULL

struct root_boot {
    uint64_t      self_tid;
    framebuffer_t fb;
    initrd_info_t initrd;
    uint64_t      pool_bytes;
    uint64_t      frame_count;
};

void rootserver_boot(void);