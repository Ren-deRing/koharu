#include <elf.h>

#include <string.h>
#include <stdint.h>

int elf_check(const void *elf, uint64_t size) {
    const struct elf64_ehdr *e = (const struct elf64_ehdr *)elf;

    if (size < sizeof(struct elf64_ehdr)) return -1;
    if (memcmp(e->e_ident, "\x7f""ELF", 4)) return -1;
    if (e->e_ident[4] != ELFCLASS64) return -1;
    if (e->e_ident[5] != ELFDATA2LSB) return -1;
    if (e->e_type != ET_EXEC) return -1;
    if (e->e_machine != EM_X86_64) return -1;
    if (e->e_phentsize != sizeof(struct elf64_phdr)) return -1;
    if (e->e_phoff + (uint64_t)e->e_phnum * sizeof(struct elf64_phdr) > size) return -1;

    return 0;
}