#include <koharu/thread.h>
#include <koharu/intc.h>
#include <koharu/acpi.h>
#include <koharu/pmap.h>
#include <koharu/assert.h>
#include <koharu/cpu.h>
#include <koharu/initcall.h>
#include <koharu/bootinfo.h>
#include <koharu/mmu.h>
#include <koharu/print.h>
#include <koharu/kmem.h>

#include <string.h>

boot_info_t* g_boot_info;

__attribute__((section("entry")))
void generic_entry(boot_info_t* boot_info) {
    g_boot_info = boot_info;

    run_initcalls(__early_initcall_start, __early_initcall_end);
    run_initcalls(__core_initcall_start, __core_initcall_end);
    run_initcalls(__arch_initcall_start, __arch_initcall_end);
    run_initcalls(__sys_initcall_start, __sys_initcall_end);
    run_initcalls(__late_initcall_start, __late_initcall_end);

    extern int elf_load(pmap_t *pmap, const void *elf, size_t size, uintptr_t *entry_out);

    pmap_t *up = pmap_create();
    uintptr_t entry;
    
    elf_load(up, (void *)g_boot_info->init.addr, g_boot_info->init.size, &entry);
    
    pmap_map(up, 0x800000, virt_to_phys(boot_info->framebuffer.address),
            (2 * 1024 * 1024), PROT_READ | PROT_WRITE | PROT_USER | PROT_WC);
    
    struct thread *t[3];
    for (int i = 0; i < 3; i++)
        t[i] = thread_create(up, entry, (void *)(uintptr_t)i, 0);

    sched_boot();

    for (;;) arch_halt();
}