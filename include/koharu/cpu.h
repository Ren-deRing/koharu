#pragma once

#include <stdint.h>

#define MAX_CPUS 255

struct cpu {
    struct cpu *self;

    uint64_t tss_rsp0;
    uint64_t user_rsp;

    uint32_t id;
    uint32_t hw_id;

    uint64_t timer_ticks_per_ms; 

    void *arch_cpu_data;
};

struct cpu* get_this_core(void);

void arch_halt();

#define curcpu get_this_core()