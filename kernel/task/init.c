#include <koharu/elf.h>
#include <koharu/mmu.h>
#include <koharu/pmap.h>
#include <koharu/string.h>

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

