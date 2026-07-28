#pragma once

#include <stdint.h>

#define MAX_CPUS 255

struct cpu {
    struct cpu *self;

    uint64_t    tss_rsp0;
    uint64_t    user_rsp;

    uint32_t    id;
    uint32_t    hw_id;

    uint64_t    timer_ticks_per_ms;

    struct control_t      *tlsf_ctrl;
    struct block_header_t *pending_free_list;

    void       *arch_cpu_data;
} __attribute__((aligned(64)));

struct cpu* get_this_core(void);
struct cpu* id_to_cpu(uint16_t cpu_id);

void arch_halt();

#define curcpu get_this_core()