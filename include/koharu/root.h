#pragma once

#include <koharu/bootinfo.h>

#include <stdint.h>

#define ROOT_BOOT_VA 0x700000001000ULL
#define ROOT_POOL_VA 0x700000000000ULL

struct root_boot {
    uint64_t      self_tid;
    framebuffer_t fb;
    uint64_t      pool_bytes;
    uint64_t      frame_count;
};

void rootserver_boot(void);