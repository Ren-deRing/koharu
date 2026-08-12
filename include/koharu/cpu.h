#pragma once

#include <stdint.h>

#define MAX_CPUS 255

typedef uint64_t cpu_status_t;

struct cpu {
    struct cpu *self;

    uint64_t    kstack_top;
    uint64_t    user_sp;

    uint32_t    id;
    uint32_t    hw_id;

    uint64_t    tsc_freq_hz;

    struct control_t      *tlsf_ctrl;
    struct block_header_t *pending_free_list;

    struct arch_cpu       *arch_cpu_data;
} __attribute__((aligned(64)));

struct cpu* get_this_core(void);
struct cpu* id_to_cpu(uint16_t cpu_id);

void arch_halt();
cpu_status_t arch_irq_save(void);
void arch_irq_restore(cpu_status_t flags);
void arch_irq_disable(void);
void arch_irq_enable(void);
#define curcpu get_this_core()