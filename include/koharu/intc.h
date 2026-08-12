#pragma once

#include <stdint.h>

struct intc_ops {
    const char *name;
    
    void (*eoi)(void);
    uint32_t (*get_id)(void);
    
    void (*timer_oneshot)(uint8_t vector, uint64_t us);
    void (*timer_periodic)(uint8_t vector, uint32_t hz);
    void (*timer_stop)(void);
    
    void (*mask_irq)(uint32_t irq);
    void (*unmask_irq)(uint32_t irq);
};

extern struct intc_ops *g_intc;

static inline void intc_eoi(void) {
    if (g_intc && g_intc->eoi) g_intc->eoi();
}

static inline uint32_t intc_get_id(void) {
    return g_intc ? g_intc->get_id() : 0;
}