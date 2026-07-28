#include <koharu/assert.h>
#include <koharu/cpu.h>
#include <koharu/initcall.h>
#include <koharu/bootinfo.h>
#include <koharu/mmu.h>
#include <koharu/print.h>
#include <koharu/kmem.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

    // uintptr_t pages[1024];
    // for (int i = 0; i < 1024; i++) {
    //     pages[i] = (uintptr_t)pmm_alloc_pages(5);
    //     if (!pages[i]) {
    //         dprintf("Failed at index %d\n", i);
    //         break;
    //     }
    //     dprintf("allocated [%d]: 0x%016lx\n", i, pages[i]);
    // }

    // for (int i = 0; i < 1024; i++) {
    //     if (pages[i]) pmm_free_pages((void*)pages[i], 5);
    // }

    void *ptr1 = pmm_alloc_pages(0);
    void *ptr2 = pmm_alloc_pages(0);
    dprintf("alloc: %p\n", ptr1);
    dprintf("alloc: %p\n", ptr2);

    void *ptr3 = pmm_alloc_pages(0);
    void *ptr4 = pmm_alloc_pages(0);
    dprintf("alloc: %p\n", ptr3);
    dprintf("alloc: %p\n", ptr4);

    char *buf1 = (char*)kmalloc(64);
    strcpy(buf1, "shimoe koharu");
    assert(strcmp(buf1, "shimoe koharu") == 0);
    dprintf("buffer says: '%s'\n", buf1);

    for (int i = 0; i < 1024; i++) {
        char *buf = kmalloc(1024);
        if (!buf) {
            dprintf("Failed at index %d\n", i);
            break;
        }
        strcpy(buf, "shimoe koharu");
        assert(strcmp(buf, "shimoe koharu") == 0);

        kfree(buf);
    }

    char *buf2 = kmalloc(1024 * 1024 * 3);
    strcpy(buf2, "shimoe koharu");
    assert(strcmp(buf2, "shimoe koharu") == 0);

    char *buf3 = kmalloc(1024 * 512);
    strcpy(buf3, "shimoe koharu");
    assert(strcmp(buf3, "shimoe koharu") == 0);

    for (;;) arch_halt();
}