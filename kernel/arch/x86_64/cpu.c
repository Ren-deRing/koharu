#include <koharu/cpu.h>
#include <koharu/initcall.h>
#include <koharu/mmu.h>

#include <asm/cpu.h>

#include <stdint.h>

#define IA32_EFER_MSR 0xC0000080
#define EFER_NXE      (1ULL << 11)

struct cpu cpus[MAX_CPUS];
struct arch_cpu arch_cpus[MAX_CPUS];

cpu_status_t arch_irq_save(void) {
    cpu_status_t flags;
    asm volatile ("pushfq; pop %0; cli" : "=rm"(flags) :: "memory");
    return flags;
}

void arch_irq_restore(cpu_status_t flags) {
    asm volatile ("push %0; popfq" : : "rm"(flags) : "memory", "cc");
}

void arch_irq_disable(void) {
    asm volatile ("cli");
}

void arch_irq_enable(void) {
    asm volatile ("sti");
}

void arch_halt() {
    asm volatile ("hlt");
}

uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ __volatile__(
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );
    return ((uint64_t)high << 32) | low;
}

void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = (uint32_t)val;
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ __volatile__(
        "wrmsr"
        :
        : "c"(msr), "a"(low), "d"(high)
    );
}

static inline void cpuid(uint32_t leaf, uint32_t subleaf,
                                uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    asm volatile("cpuid"
                 : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                 : "a"(leaf), "c"(subleaf));
}

static inline int is_intel(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    // ebx == "Genu", edx == "ineI", ecx == "ntel"
    return (ebx == 0x756e6547 && edx == 0x49656e69 && ecx == 0x6c65746e);
}

static inline uint32_t get_apic_id(void) {
    uint32_t eax, ebx, ecx, edx;
    asm volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    return (ebx >> 24) & 0xFF; // APIC ID here!
}

void init_cpu_gs(struct cpu* cpu_ptr) {
    uint64_t addr = (uint64_t)cpu_ptr;
    // IA32_GS_BASE MSR = 0xC0000101
    asm volatile ("wrmsr" : : "c"(0xC0000101), "a"((uint32_t)addr), "d"((uint32_t)(addr >> 32)): "memory"); // gs base
    asm volatile ("wrmsr" : : "c"(0xC0000102), "a"((uint32_t)addr), "d"((uint32_t)(addr >> 32)): "memory"); // kernel gs base
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

    c->arch_cpu_data = &arch_cpus[my_id];

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

int enable_smepsmap(void) {
    uintptr_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 20); // SMEP
    cr4 |= (1ULL << 21); // SMAP
    asm volatile("mov %0, %%cr4" :: "r"(cr4));

    return 0;
}

int enable_xsaves(void) {
    uint32_t eax, ebx, ecx, edx;

    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    if (!(ecx & (1 << 26))) return -1;

    // CR4.OSXSAVE
    uint64_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 18);
    asm volatile("mov %0, %%cr4" :: "r"(cr4));

    uint32_t xcr0_lo = 0x7; // x87 + SSE + AVX
    asm volatile("xsetbv" :: "a"(xcr0_lo), "d"(0), "c"(0));

    cpuid(0xD, 1, &eax, &ebx, &ecx, &edx);
    if (!(eax & (1 << 3))) return -1;

    uint32_t xss_lo = 0;
    uint32_t xss_hi = 0;
    asm volatile("wrmsr" :: "a"(xss_lo), "d"(xss_hi), "c"(0x00000DA0)); // MSR_IA32_XSS

    curcpu->arch_cpu_data->xsave_size = ebx;

    return 0;
}

int enable_fsgsbase(void) {
    uint32_t eax, ebx, ecx, edx;

    cpuid(7, 0, &eax, &ebx, &ecx, &edx);
    if (!(ebx & (1 << 0))) return -1;
    
    uint64_t cr4;
    
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 16);
    asm volatile("mov %0, %%cr4" :: "r"(cr4));

    return 0;
}

int enable_x2apic(void) {
    uint32_t eax, ebx, ecx, edx;

    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    if (!(ecx & (1 << 21))) return -1;

    uint64_t apic_base = rdmsr(0x1B);

    if ((apic_base & (3ULL << 10)) == (3ULL << 10)) {
        return 0;
    }

    apic_base |= (1ULL << 11) | (1ULL << 10); // Enable & x2APIC Mode
    wrmsr(0x1B, apic_base);

    return 0;
}

int set_tsc_frequency(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;

    // hypervisor
    cpuid(0x40000010, 0, &eax, &ebx, &ecx, &edx);
    if (eax != 0) {
        // eax: tsc freq (kHz)
        curcpu->tsc_freq_hz = (uint64_t)eax * 1000ULL;
        return 0;
    }

    // tsc / clock info
    cpuid(0x15, 0, &eax, &ebx, &ecx, &edx);
    if (eax != 0 && ebx != 0 && ecx != 0) {
        curcpu->tsc_freq_hz = ((uint64_t)ecx * ebx) / eax;
        return 0;
    }

    if (is_intel()) {
        // intel base freq
        cpuid(0x16, 0, &eax, &ebx, &ecx, &edx);
        if (eax != 0) {
            curcpu->tsc_freq_hz = (uint64_t)eax * 1000000ULL;
            dprintf("tsc freq (Intel CPUID 0x16): %lu Hz\n", curcpu->tsc_freq_hz);
            return 0;
        }

        // intel platform info
        uint64_t platform_info = rdmsr(0xCE);
        uint64_t ratio = (platform_info >> 8) & 0xFF;
        if (ratio != 0) {
            curcpu->tsc_freq_hz = ratio * 100000000ULL; // base clock 100MHz
            dprintf("tsc freq (Intel MSR 0xCE): %lu Hz\n", curcpu->tsc_freq_hz);
            return 0;
        }
    }
    else {
        uint64_t pstate0 = rdmsr(0xC0010064);
        if (pstate0 & (1ULL << 63)) { // P-state valid
            uint64_t cpu_fid = (pstate0 >> 8) & 0x3F;
            uint64_t cpu_did = pstate0 & 0x3F;
            if (cpu_did != 0) {
                // clock = (200 * cpuFid) / cpuDid (MHz)
                uint64_t freq_mhz = (200 * cpu_fid) / cpu_did;
                curcpu->tsc_freq_hz = freq_mhz * 1000000ULL;
                dprintf("tsc freq (AMD P-State MSR): %lu Hz\n", curcpu->tsc_freq_hz);
                return 0;
            }
        }
    }

    return -1; // what is this?
}

early_initcall(init_percpu, A2);
early_initcall(enable_nx, A3);
early_initcall(enable_pge, A4);
early_initcall(enable_smepsmap, A5);
early_initcall(enable_xsaves, A6);
early_initcall(enable_fsgsbase, A7);
early_initcall(enable_x2apic, A8);
early_initcall(set_tsc_frequency, A9);