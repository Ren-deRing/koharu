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

    if (g_boot_info->init.size > 0) {
        const uint8_t *p = (const uint8_t *)g_boot_info->init.addr;
        uint32_t pw = 0, ph = 0;

        p += 3; // skip "P6\n"
        while (*p >= '0' && *p <= '9') pw = pw * 10 + *p++ - '0';
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        while (*p >= '0' && *p <= '9') ph = ph * 10 + *p++ - '0';
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        while (*p >= '0' && *p <= '9') p++; // skip maxval
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        // data start

        uint32_t fw = boot_info->framebuffer.width;
        uint32_t fh = boot_info->framebuffer.height;
        uint32_t fs = boot_info->framebuffer.pitch / 4;

        uint32_t ow = fw, oh = (uint32_t)((uint64_t)ph * fw / pw);
        if (oh > fh) { oh = fh; ow = (uint32_t)((uint64_t)pw * fh / ph); }
        int ox = ((int)fw - (int)ow) / 2;
        int oy = ((int)fh - (int)oh) / 2;

        for (uint32_t y = 0; y < oh; y++) {
            uint32_t sy = (uint32_t)(((uint64_t)y * ph) / oh);
            const uint8_t *srow = p + (size_t)sy * pw * 3;
            uint32_t *row = fb + (size_t)(oy + (int)y) * fs;
            for (uint32_t x = 0; x < ow; x++) {
                uint32_t sx = (uint32_t)(((uint64_t)x * pw) / ow);
                const uint8_t *px = srow + sx * 3;
                row[ox + (int)x] = ((uint32_t)px[0] << 16) | ((uint32_t)px[1] << 8) | px[2];
            }
        }
    }

    pmap_t *test = pmap_create();
    pmap_map(test, 0x400000, 0x100000, PAGE_SIZE * 3, PROT_READ | PROT_WRITE | PROT_USER);
    pmap_unmap(test, 0x400000, PAGE_SIZE * 3);
    pmap_destroy(test);

    dprintf("current g_intc implementation: %s\n", g_intc->name);

    for (;;) arch_halt();
}