#include <koharu/pmap.h>
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

    run_initcalls(__early_initcall_start, __early_initcall_end);
    run_initcalls(__core_initcall_start, __core_initcall_end);
    run_initcalls(__arch_initcall_start, __arch_initcall_end);

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

    dprintf("allocating %dB\n", 1024 * 1024 * 3);

    char *buf2 = kmalloc(1024 * 1024 * 3);
    strcpy(buf2, "shimoe koharu");
    assert(strcmp(buf2, "shimoe koharu") == 0);

    dprintf("allocating %dB\n", 1024 * 512);

    char *buf3 = kmalloc(1024 * 512);
    strcpy(buf3, "shimoe koharu");
    assert(strcmp(buf3, "shimoe koharu") == 0);

    uint32_t* fb = boot_info->framebuffer.address;

    pmap_map(pmap_kernel(), (uint64_t)fb, virt_to_phys(fb),
        (g_boot_info->framebuffer.height * g_boot_info->framebuffer.pitch), PROT_READ | PROT_WRITE | PROT_WC | PROT_GLOBAL);

    uint32_t total_pixels = (boot_info->framebuffer.pitch / 4) * boot_info->framebuffer.height;
    
    for (uint32_t i = 0; i < total_pixels; i++) {
        fb[i] = 0xCC9BA3; // KOHARU
    }

    pmap_unmap(pmap_kernel(), (uint64_t)fb, (g_boot_info->framebuffer.height * g_boot_info->framebuffer.pitch));

    pmap_map(pmap_kernel(), (uint64_t)fb, virt_to_phys(fb),
        (g_boot_info->framebuffer.height * g_boot_info->framebuffer.pitch), PROT_READ | PROT_WRITE | PROT_WC | PROT_GLOBAL);

    for (uint32_t i = 0; i < total_pixels; i++) {
        fb[i] = 0xF2BDCB; // KOHARU (swinsuit)
    }

    pmap_t *test = pmap_create();
    pmap_map(test, 0x400000, 0x100000, PAGE_SIZE * 3, PROT_READ | PROT_WRITE | PROT_USER);
    pmap_unmap(test, 0x400000, PAGE_SIZE * 3);
    pmap_destroy(test);

    for (;;) arch_halt();
}