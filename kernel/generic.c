#include <koharu/cpu.h>
#include <koharu/initcall.h>
#include <koharu/bootinfo.h>
#include <koharu/mmu.h>
#include <koharu/print.h>
#include <stddef.h>
#include <stdint.h>

boot_info_t* g_boot_info;

__attribute__((section("entry")))
void generic_entry(boot_info_t* boot_info) {
    g_boot_info = boot_info;

    volatile uint32_t* fb = boot_info->screen.fb;
    uint32_t total_pixels = (boot_info->screen.pitch / 4) * boot_info->screen.height;
    
    for (uint32_t i = 0; i < total_pixels; i++) {
        fb[i] = 0xCC9BA3; // KOHARU
    }

    run_initcalls(__early_initcall_start, __early_initcall_end);
    run_initcalls(__arch_initcall_start, __arch_initcall_end);
    run_initcalls(__core_initcall_start, __core_initcall_end);

    for (size_t i = 0; i < g_boot_info->memory.count; i++) {
        dprintf("  [%02lu] Base: 0x%016lx | Length: 0x%016lx | Type: %u\n",
                i,
                g_boot_info->memory.entries[i].base_addr,
                g_boot_info->memory.entries[i].length,
                g_boot_info->memory.entries[i].type);
    }

    dprintf("faulting myself..\n");

    asm volatile ("int $0x3");

    for (int i = 0; i < 1024; i++) {
        uintptr_t page = (uintptr_t)pmm_alloc_pages(10);
        if (!page) {
            dprintf("Failed at index %d\n", i);
            break;
        }
        dprintf("allocated [%d]: 0x%lx\n", i, page);
        pmm_free_pages((void*)page, 10);
    }

    void *ptr1 = pmm_alloc_pages(0);
    void *ptr2 = pmm_alloc_pages(0);
    dprintf("alloc: %p\n", ptr1);
    dprintf("alloc: %p\n", ptr2);

    void *ptr3 = pmm_alloc_pages(0);
    void *ptr4 = pmm_alloc_pages(0);
    dprintf("alloc: %p\n", ptr3);
    dprintf("alloc: %p\n", ptr4);

    for (;;) arch_halt();
}