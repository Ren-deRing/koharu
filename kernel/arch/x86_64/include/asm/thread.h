#pragma once

#include <stdint.h>

struct arch_thread {
    uint64_t rsp;
    uint64_t fs_base;
    void    *xsaves_area;
};