#include <console.h>
#include <cpio.h>
#include <elf.h>
#include <pager.h>
#include <root.h>
#include <syscall.h>

#include <stddef.h>
#include <stdint.h>

void root_main(uint64_t arg) {
    (void)arg;

    struct root_boot *boot = (struct root_boot *)ROOT_BOOT_VA;

    pager_init(boot->self_tid, boot->frame_count);

    log_str("[root] tid=");
    log_ulong(boot->self_tid);
    log_str("\n");

    log_str("[root] pool ");
    log_ulong(boot->pool_bytes);
    log_str(" bytes / ");
    log_ulong(boot->frame_count);
    log_str(" frames\n");

    log_str("[root] initrd ");
    log_hex(boot->initrd.addr);
    log_str(" size=");
    log_ulong(boot->initrd.size);
    log_str("\n");

    if (boot->initrd.addr == 0 || boot->initrd.size == 0) {
        log_str("[root] no initrd\n");
        serve(0);
    }

    uint64_t elf_base;
    size_t elf_size;
    if (cpio_extract((void *)boot->initrd.addr, "child.elf", &elf_base, &elf_size) != 0) {
        log_str("[root] child.elf not found\n");
        serve(0);
    }

    log_str("[root] child.elf ");
    log_hex(elf_base);
    log_str(" size=");
    log_ulong(elf_size);
    log_str("\n");

    const uint8_t *elf = (const uint8_t *)elf_base;

    if (elf_check(elf, elf_size) != 0) {
        log_str("[root] bad elf\n");
        serve(0);
    }

    const struct elf64_ehdr *ehdr = (const struct elf64_ehdr *)elf;

    long child = syscall5(SYS_THREAD_CONTROL, TC_CREATE, 0, 0, boot->self_tid, 0);
    if (child <= 0) {
        log_str("[root] child create failed\n");
        serve(0);
    }

    log_str("[root] child tid=");
    log_ulong((uint64_t)child);
    log_str("\n");

    const struct elf64_phdr *phdr = (const struct elf64_phdr *)(elf + ehdr->e_phoff);
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;

        struct elf64_phdr ph = phdr[i];

        while (i + 1 < ehdr->e_phnum &&
               phdr[i + 1].p_type == PT_LOAD &&
               phdr[i + 1].p_flags == ph.p_flags &&
               phdr[i + 1].p_vaddr == ph.p_vaddr + ph.p_memsz &&
               phdr[i + 1].p_offset == ph.p_offset + ph.p_filesz) {
            ph.p_memsz  = phdr[i + 1].p_vaddr + phdr[i + 1].p_memsz - ph.p_vaddr;
            ph.p_filesz = phdr[i + 1].p_offset + phdr[i + 1].p_filesz - ph.p_offset;
            i++;
        }

        if (load_segment((uint64_t)child, elf, &ph) != 0) {
            log_str("[root] segment load failed\n");
            serve((uint64_t)child);
        }
    }

    if (map_stack((uint64_t)child) != 0) {
        log_str("[root] stack map failed\n");
        serve((uint64_t)child);
    }

    if (map_shared((uint64_t)child) != 0) {
        log_str("[root] shared map failed\n");
        serve((uint64_t)child);
    }

    if (map_utcb((uint64_t)child, (uint64_t)child, boot->self_tid) != 0) {
        log_str("[root] utcb map failed\n");
        serve((uint64_t)child);
    }

    if (syscall5(SYS_EXCHANGE_REGISTERS, (uint64_t)child, ehdr->e_entry, UTBC_VA, UTBC_VA, EXR_SET_ENTRY) != 0) {
        log_str("[root] child register write failed\n");
        serve((uint64_t)child);
    }
    if (syscall5(SYS_EXCHANGE_REGISTERS, (uint64_t)child, 0, 0, 0, EXR_ACTIVATE) != 0) {
        log_str("[root] child start failed\n");
        serve((uint64_t)child);
    }

    log_str("[root] child started\n");

    serve((uint64_t)child);

    for (;;);
}