#include <koharu/initcall.h>
#include <koharu/print.h>

#include <asm/trapframe.h>
#include <idt.h>

#include <stdbool.h>

typedef struct {
    uint16_t isr_low;
    uint16_t kernel_cs;
    uint8_t  ist;
    uint8_t  attributes; 
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

void isr_handler(struct trapframe *tf) {
    dprintf("\nPANIC! [#%02lu] [E%lu]\n", tf->vector, tf->error);
    dprintf("  CS:RIP = 0x%04lx:0x%016lx\n", tf->cs, tf->rip);
    dprintf("  RAX:     0x%016lx  RBX: 0x%016lx\n", tf->rax, tf->rbx);
    dprintf("  RCX:     0x%016lx  RDX: 0x%016lx\n", tf->rcx, tf->rdx);
    dprintf("  RSP:     0x%016lx  RBP: 0x%016lx\n", tf->rsp, tf->rbp);
    dprintf("  RFLAGS:  0x%016lx\n", tf->rflags);

    if (tf->vector != 3) {
        dprintf("oh no, halting.\n");
        for (;;) arch_halt();
    }

    dprintf("that was not a panic.\n\n");
}

static volatile idt_entry_t idt[256];
static volatile idtr_t idtr;

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags, uint8_t ist) {
    uintptr_t addr = (uintptr_t)isr;
    idt[vector].isr_low    = addr & 0xFFFF;
    idt[vector].kernel_cs  = 0x08;       // GDT's kernel CS
    idt[vector].ist        = ist & 0x07; // low 3
    idt[vector].attributes = flags;
    idt[vector].isr_mid    = (addr >> 16) & 0xFFFF;
    idt[vector].isr_high   = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved   = 0;
}

int init_idt() {
    idt_set_descriptor(0,  isr0,  0x8E, 0);
	idt_set_descriptor(1,  isr1,  0x8E, 0);
	idt_set_descriptor(2,  isr2,  0x8E, 0);
	idt_set_descriptor(3,  isr3,  0x8E, 0);
	idt_set_descriptor(4,  isr4,  0x8E, 0);
	idt_set_descriptor(5,  isr5,  0x8E, 0);
	idt_set_descriptor(6,  isr6,  0x8E, 0);
	idt_set_descriptor(7,  isr7,  0x8E, 0);
	idt_set_descriptor(8,  isr8,  0x8E, 1); // DF IST
	idt_set_descriptor(9,  isr9,  0x8E, 0);
	idt_set_descriptor(10, isr10, 0x8E, 0);
	idt_set_descriptor(11, isr11, 0x8E, 0);
	idt_set_descriptor(12, isr12, 0x8E, 0);
	idt_set_descriptor(13, isr13, 0x8E, 0);
	idt_set_descriptor(14, isr14, 0x8E, 0);
	idt_set_descriptor(15, isr15, 0x8E, 0);
	idt_set_descriptor(16, isr16, 0x8E, 0);
	idt_set_descriptor(17, isr17, 0x8E, 0);
	idt_set_descriptor(18, isr18, 0x8E, 0);
	idt_set_descriptor(19, isr19, 0x8E, 0);
	idt_set_descriptor(20, isr20, 0x8E, 0);
	idt_set_descriptor(21, isr21, 0x8E, 0);
	idt_set_descriptor(22, isr22, 0x8E, 0);
	idt_set_descriptor(23, isr23, 0x8E, 0);
	idt_set_descriptor(24, isr24, 0x8E, 0);
	idt_set_descriptor(25, isr25, 0x8E, 0);
	idt_set_descriptor(26, isr26, 0x8E, 0);
	idt_set_descriptor(27, isr27, 0x8E, 0);
	idt_set_descriptor(28, isr28, 0x8E, 0);
	idt_set_descriptor(29, isr29, 0x8E, 0);
	idt_set_descriptor(30, isr30, 0x8E, 0);
	idt_set_descriptor(31, isr31, 0x8E, 0);

    return 0;
}

static volatile bool is_table_initialized = false;

int load_idt() {
    if (!is_table_initialized) init_idt(); // same thing as gdt
    is_table_initialized = true;

    // IDTR settings...
    idtr.limit = sizeof(idt) - 1;
	idtr.base  = (uintptr_t)&idt;

    // load idt!
    asm volatile ("lidt %0" : : "m"(idtr));

    return 0;
}

early_initcall(load_idt, B1);