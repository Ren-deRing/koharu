#include <koharu/intc.h>
#include <koharu/initcall.h>
#include <koharu/print.h>
#include <koharu/cpu.h>

#include <asm/cpu.h> 

#define IA32_X2APIC_ID         0x802
#define IA32_X2APIC_VER        0x803
#define IA32_X2APIC_TPR        0x808 // task priority
#define IA32_X2APIC_EOI        0x80B
#define IA32_X2APIC_LDR        0x80D // logical destination
#define IA32_X2APIC_SIVR       0x80F
#define IA32_X2APIC_ESR        0x828 // error status
#define IA32_X2APIC_ICR        0x830 // interrupt command register
#define IA32_X2APIC_LVT_TIMER  0x832
#define IA32_X2APIC_TIMER_INIT 0x838
#define IA32_X2APIC_TIMER_CUR  0x839
#define IA32_X2APIC_TIMER_DIV  0x83E // divide configuration

#define LAPIC_TIMER_ONESHOT      (0b00 << 17)
#define LAPIC_TIMER_PERIODIC     (0b01 << 17)
#define LAPIC_TIMER_TSC_DEADLINE (0b10 << 17)

#define LAPIC_TIMER_MASKED       (1 << 16)

void x2apic_eoi(void) {
    wrmsr(IA32_X2APIC_EOI, 0);
}

uint32_t x2apic_get_id(void) {
    return (uint32_t)rdmsr(IA32_X2APIC_ID);
}

void lapic_timer_stop(void) {
    wrmsr(IA32_X2APIC_LVT_TIMER, LAPIC_TIMER_MASKED);
}

void lapic_timer_oneshot(uint8_t vector, uint64_t us) {
    lapic_timer_stop();

    wrmsr(IA32_X2APIC_TIMER_DIV, 0x0B);

    uint64_t ticks = (curcpu->tsc_freq_hz / 1000000ULL) * us;

    wrmsr(IA32_X2APIC_TIMER_INIT, (uint32_t)ticks);
    wrmsr(IA32_X2APIC_LVT_TIMER, LAPIC_TIMER_ONESHOT | vector);
}

void lapic_timer_periodic(uint8_t vector, uint32_t hz) {
    lapic_timer_stop();

    wrmsr(IA32_X2APIC_TIMER_DIV, 0x0B);

    uint64_t ticks = curcpu->tsc_freq_hz / hz;

    wrmsr(IA32_X2APIC_TIMER_INIT, (uint32_t)ticks);
    wrmsr(IA32_X2APIC_LVT_TIMER, LAPIC_TIMER_PERIODIC | vector);
}

int lapic_init(void) {
    wrmsr(IA32_X2APIC_TPR, 0);

    uint64_t sivr = rdmsr(IA32_X2APIC_SIVR);
    sivr |= (1ULL << 8) | 0xFF; 
    wrmsr(IA32_X2APIC_SIVR, sivr);

    return 0;
}

static struct intc_ops x2apic_ops = {
    .name = "x2APIC",
    .eoi = x2apic_eoi,
    .get_id = x2apic_get_id,
    
    .timer_oneshot = lapic_timer_oneshot,
    .timer_periodic = lapic_timer_periodic,
    .timer_stop = lapic_timer_stop,
};

struct intc_ops *g_intc = &x2apic_ops;

arch_initcall(lapic_init, B0);