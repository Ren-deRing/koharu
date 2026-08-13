#include <koharu/bootinfo.h>
#include <koharu/elf.h>
#include <koharu/initcall.h>
#include <koharu/string.h>
#include <koharu/pmap.h>

#include <koharu/mmu.h>
#include <stdbool.h>
#include <stdint.h>

int map_kernel(uint64_t kernel_src) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)kernel_src;

    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
        return -1; // wrong elf magic
    }

    Elf64_Shdr *shdr = (Elf64_Shdr *)(kernel_src + ehdr->e_shoff);

    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *sec = &shdr[i];

        if (!(sec->sh_flags & SHF_ALLOC)) continue; // we are mapping a memory now. not a elf headers.
        if (sec->sh_size == 0) continue;

        uint64_t vaddr = sec->sh_addr;
        uint64_t size  = sec->sh_size;
        uint64_t flags = sec->sh_flags;

        // flags
        bool is_writeable = (flags & SHF_WRITE) != 0;
        bool is_executable = (flags & SHF_EXECINSTR) != 0;

        uint32_t map_flags = PROT_READ | PROT_GLOBAL;
        if (is_writeable)  map_flags |= PROT_WRITE;
        if (is_executable) map_flags |= PROT_EXEC;

        uintptr_t paddr = vaddr - 0xFFFFFFFF80000000ULL;

        int err = pmap_map(pmap_kernel(), vaddr, paddr, size, map_flags);
        if (err != 0) return err;
    }

    return 0;
}

int vmm_init() {
    pmap_init();

    // first, map entire memory.
    int err = pmap_map(pmap_kernel(), 0xFFFF800000000000ULL, 0,
       g_boot_info->memory.max_phys_addr, PROT_READ | PROT_WRITE | PROT_HUGE | PROT_GLOBAL);
    if (err != 0) return err;

    // next, map kernel addr.
    err = map_kernel(g_boot_info->kernel.kernel_src);
    if (err != 0) return err;

    // and activate
    pmap_activate(pmap_kernel());

    return 0;
}

core_initcall(vmm_init, 2);