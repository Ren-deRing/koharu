#include <koharu/cpu.h>
#include <koharu/initcall.h>

#include <asm/cpu.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_mid;
	uint8_t access;
	uint8_t flags;
	uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t length;
    uint16_t base_low;
    uint8_t  base_middle1;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_middle2;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed)) tss_descriptor_t;

typedef struct tss_entry {
	uint32_t koharu;
	uint64_t rsp[3];
	uint64_t is;
	uint64_t ist[7];
	uint64_t good;
	uint16_t isntit;
	uint16_t iomap_offset;
} __attribute__ ((packed)) tss_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;

typedef struct {
    gdt_entry_t entries[7]; // NULL, 0Code, 0Data, 3Code, 3Data, TSS LO, TSS HI, TSS
    gdtr_t      pointer;
    tss_entry_t tss;
    uint8_t df_stack[8192] __attribute__((aligned(16)));
    uint8_t kstack[16384] __attribute__((aligned(16))); 
} __attribute__((packed)) GDT;

GDT gdt[MAX_CPUS];

static inline void set_gdt_entry(gdt_entry_t* entry, uint8_t access, uint8_t flags) {
    entry->limit_low = 0xFFFF;
    entry->base_low = 0;
    entry->base_mid = 0;
    entry->access = access;
    entry->flags = flags;
    entry->base_high = 0;
}

static inline void gdt_load(gdtr_t* ptr) {
    asm volatile (
        "lgdt %0\n"
        "pushq $0x08\n"               // kernel code
        "leaq 1f(%%rip), %%rax\n"     // label to return
        "push %%rax\n"
        "lretq\n"                     // load & new rip!
        "1:\n"
        "mov $0x10, %%ax\n"           // kernel data
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "mov $0x28, %%ax\n"           // tss
        "ltr %%ax\n"
        : : "m"(*ptr) : "rax", "memory"
    );
}

int init_gdt() {
    extern struct arch_cpu arch_cpus[];

    for (int i = 0; i < MAX_CPUS; i++) {
        set_gdt_entry(&gdt[i].entries[1], 0x9A, 0x20); // kernel code (0x08)
        set_gdt_entry(&gdt[i].entries[2], 0x92, 0x00); // kernel data (0x10)
        set_gdt_entry(&gdt[i].entries[3], 0xF3, 0x00); // user data   (0x1B)
        set_gdt_entry(&gdt[i].entries[4], 0xFA, 0x20); // user code   (0x23)

        // TSS
        tss_descriptor_t* tss_desc = (tss_descriptor_t*)&gdt[i].entries[5];

        uint64_t tss_base = (uint64_t)&gdt[i].tss;
        uint32_t tss_limit = sizeof(tss_entry_t) - 1;

        tss_desc->length       = tss_limit & 0xFFFF;
        tss_desc->base_low     = tss_base & 0xFFFF;
        tss_desc->base_middle1 = (tss_base >> 16) & 0xFF;
        tss_desc->flags1       = 0x89; // present, 64-bits TSS (avail)
        tss_desc->flags2       = (tss_limit >> 16) & 0x0F;
        tss_desc->base_middle2 = (tss_base >> 24) & 0xFF;
        tss_desc->base_high    = (tss_base >> 32) & 0xFFFFFFFF;
        tss_desc->reserved     = 0;

        // IST1 (for #DF)
        gdt[i].tss.ist[0] = (uintptr_t)gdt[i].df_stack + sizeof(gdt[i].df_stack);

        // kstack
        gdt[i].tss.rsp[0] = (uintptr_t)gdt[i].kstack + sizeof(gdt[i].kstack);

        arch_cpus[i].tss = &gdt[i].tss;

        // LGDT
        gdt[i].pointer.limit = sizeof(gdt[i].entries) - 1;
        gdt[i].pointer.base  = (uintptr_t)&gdt[i].entries[0];
    }

    return 0;
}

static volatile bool is_table_initialized = false;

int load_gdt() {
    if (!is_table_initialized) init_gdt(); // BSP is not initialized simultaneously with the APs!
    is_table_initialized = true;           // so, volatile is sufficient.

    // load gdt!
    gdt_load(&gdt[curcpu->id].pointer);

    return 0;
}

early_initcall(load_gdt, B0);