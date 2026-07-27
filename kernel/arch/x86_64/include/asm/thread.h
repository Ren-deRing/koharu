#pragma once

#include <stdint.h>

struct arch_thread {
    uintptr_t  rsp;
    uintptr_t  cr3;
    void      *kernel_stack_top;

    void      *xsaves_area;
};