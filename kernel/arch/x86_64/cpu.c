#include <koharu/cpu.h>
#include <koharu/initcall.h>
#include <koharu/mmu.h>

#include <stdint.h>

#define IA32_EFER_MSR 0xC0000080
#define EFER_NXE      (1ULL << 11)

struct cpu cpus[MAX_CPUS];

void arch_halt() {
    asm volatile ("hlt");
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ __volatile__(
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = (uint32_t)val;
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ __volatile__(
        "wrmsr"
        :
        : "c"(msr), "a"(low), "d"(high)
    );
}

static inline uint32_t get_apic_id(void) {
    uint32_t eax, ebx, ecx, edx;
    asm volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    return (ebx >> 24) & 0xFF; // APIC ID here!
}

void init_cpu_gs(struct cpu* cpu_ptr) {
    uint64_t addr = (uint64_t)cpu_ptr;
    // IA32_GS_BASE MSR = 0xC0000101
    asm volatile ("wrmsr" : : "c"(0xC0000101), "a"((uint32_t)addr), "d"((uint32_t)(addr >> 32)): "memory");
}

struct cpu* get_this_core(void) {
    struct cpu* ptr;
    asm volatile ("movq %%gs:0, %0" : "=r"(ptr)); // struct cpu located at gs[0]
    return ptr;
}

struct cpu* id_to_cpu(uint16_t cpu_id) {
    return &cpus[cpu_id];
}

volatile uint32_t logic_id = 0;

int init_percpu() {
    uint32_t hw_id = get_apic_id();
    uint32_t my_id = __atomic_fetch_add(&logic_id, 1, 5); // __ATOMIC_SEQ_CST (strictest memory model)
    // this code is run by multiple processors simultaneously.
    // that means, logic_id could be corrupted! so atomic is used.

    struct cpu *c = &cpus[my_id];

    c->self  = c;
    c->id    = my_id;
    c->hw_id = hw_id;

    init_cpu_gs(c);

    return 0;
}

int enable_nx(void) { // Non-Executable
    uint64_t efer = rdmsr(IA32_EFER_MSR);
    efer |= EFER_NXE;
    wrmsr(IA32_EFER_MSR, efer);

    return 0;
}

int enable_pge(void) { // Page Global
    uintptr_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 7); // PGE
    asm volatile("mov %0, %%cr4" :: "r"(cr4));

    return 0;
}

early_initcall(init_percpu, A2);
early_initcall(enable_nx, A3);
early_initcall(enable_pge, A4);