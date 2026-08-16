#include <koharu/cpu.h>
#include <koharu/elf.h>
#include <koharu/grant.h>
#include <koharu/mmu.h>
#include <koharu/pmap.h>
#include <koharu/print.h>
#include <koharu/root.h>
#include <koharu/thread.h>
#include <koharu/string.h>

static void boot_fail(const char *msg) {
    dprintf("rootserver_boot: %s failed\n", msg);
    for (;;) arch_halt();
}

int elf_load(pmap_t *pmap, const void *elf, size_t size, uintptr_t *entry_out) {
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)elf;

    if (size < sizeof(Elf64_Ehdr)) return -1;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) return -1;
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) return -1;
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) return -1;
    if (ehdr->e_type != ET_EXEC) return -1;
    if (ehdr->e_machine != EM_X86_64) return -1;
    if (ehdr->e_phentsize != sizeof(Elf64_Phdr)) return -1;
    if (ehdr->e_phoff + (uint64_t)ehdr->e_phnum * sizeof(Elf64_Phdr) > size) return -1;

    const Elf64_Phdr *phdr = (const Elf64_Phdr *)((const uint8_t *)elf + ehdr->e_phoff);

    for (Elf64_Half i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;
        if (phdr[i].p_offset + phdr[i].p_filesz > size) return -1;

        uint32_t prot = PROT_USER;
        if (phdr[i].p_flags & PF_R) prot |= PROT_READ;
        if (phdr[i].p_flags & PF_W) prot |= PROT_WRITE;
        if (phdr[i].p_flags & PF_X) prot |= PROT_EXEC;

        uintptr_t va     = phdr[i].p_vaddr & ~(PAGE_SIZE - 1);
        uintptr_t va_end = ALIGN_UP(phdr[i].p_vaddr + phdr[i].p_memsz, PAGE_SIZE);

        for (; va < va_end; va += PAGE_SIZE) {
            uintptr_t phys = (uintptr_t)pmm_alloc_pages(0);
            if (!phys) return -1;
            if (pmap_map(pmap, va, phys, PAGE_SIZE, prot) != 0) return -1;

            uint8_t *page = (uint8_t *)phys_to_virt(phys);

            uintptr_t f_lo = va > phdr[i].p_vaddr ? va : phdr[i].p_vaddr;
            uintptr_t f_hi = (va + PAGE_SIZE) < (phdr[i].p_vaddr + phdr[i].p_filesz)
                           ? (va + PAGE_SIZE) : (phdr[i].p_vaddr + phdr[i].p_filesz);
            if (f_hi > f_lo) {
                memcpy(page + (f_lo - va),
                       (const uint8_t *)elf + phdr[i].p_offset + (f_lo - phdr[i].p_vaddr),
                       f_hi - f_lo);
            }

            uintptr_t z_lo = va > (phdr[i].p_vaddr + phdr[i].p_filesz)
                           ? va : (phdr[i].p_vaddr + phdr[i].p_filesz);
            uintptr_t z_hi = (va + PAGE_SIZE) < (phdr[i].p_vaddr + phdr[i].p_memsz)
                           ? (va + PAGE_SIZE) : (phdr[i].p_vaddr + phdr[i].p_memsz);
            if (z_hi > z_lo) {
                memset(page + (z_lo - va), 0, z_hi - z_lo);
            }
        }
    }

    *entry_out = ehdr->e_entry;
    return 0;
}

void rootserver_boot(void) {
    pmap_t *pmap = pmap_create();
    if (!pmap) boot_fail("pmap_create");

    uintptr_t boot_pg = (uintptr_t)pmm_alloc_pages(0);
    if (!boot_pg) boot_fail("boot page");

    struct root_boot *boot = (struct root_boot *)phys_to_virt(boot_pg);
    memset(boot, 0, sizeof(*boot));

    uintptr_t entry;
    if (elf_load(pmap, (const void *)g_boot_info->init.addr, g_boot_info->init.size, &entry) != 0)
        boot_fail("elf_load");

    grant_set_root(pmap);

    uintptr_t fb_phys = 0;
    if (g_boot_info->framebuffer.address) {
        fb_phys = virt_to_phys((void *)g_boot_info->framebuffer.address);

        size_t fb_bytes = (size_t)g_boot_info->framebuffer.height
                        * g_boot_info->framebuffer.pitch * 4;
        for (uintptr_t pa = fb_phys; pa < fb_phys + fb_bytes; pa += PAGE_SIZE) {
            if (grant_add(pmap, phys_to_pfn(pa), GRANT_READ | GRANT_WRITE | GRANT_GRANT) != 0)
                boot_fail("grant fb");
        }
    }

    boot->fb.address = (uint32_t *)fb_phys;
    boot->fb.width   = g_boot_info->framebuffer.width;
    boot->fb.height  = g_boot_info->framebuffer.height;
    boot->fb.pitch   = g_boot_info->framebuffer.pitch;

    struct thread *t = thread_create(pmap, entry, (void *)ROOT_BOOT_VA, 0);
    if (!t) boot_fail("thread_create");
    boot->self_tid = t->tid;

    grant_pool_build();
    boot->pool_bytes  = grant_pool_bytes();
    boot->frame_count = grant_frame_count();

    if (pmap_map(pmap, ROOT_BOOT_VA, boot_pg, PAGE_SIZE, PROT_READ | PROT_USER) != 0)
        boot_fail("map boot");
    if (pmap_map(pmap, ROOT_POOL_VA, grant_pool_phys(), grant_pool_bytes(),
                 PROT_READ | PROT_WRITE | PROT_USER) != 0)
        boot_fail("map pool");

    size_t initrd_size = g_boot_info->initrd.size;
    if (initrd_size == 0) {
        boot->initrd.addr = 0;
        boot->initrd.size = 0;
    } else {
        uintptr_t initrd_phys = virt_to_phys((void *)g_boot_info->initrd.addr);
        if (pmap_map(pmap, ROOT_INITRD_VA, initrd_phys,
                     ALIGN_UP(initrd_size, PAGE_SIZE), PROT_READ | PROT_USER) != 0)
            boot_fail("map initrd");

        boot->initrd.addr = ROOT_INITRD_VA + (initrd_phys & (PAGE_SIZE - 1));
        boot->initrd.size = initrd_size;
    }
}
