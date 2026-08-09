#include <koharu/print.h>
#include <koharu/mmu.h>
#include <koharu/bootinfo.h>
#include <koharu/acpi.h>
#include <koharu/initcall.h>
#include <koharu/pmap.h>

int acpi_init() {
    dprintf("rsdp: %lx\n", g_boot_info->rsdp_addr);
    uintptr_t rsdp_virt = g_boot_info->rsdp_addr;
    uintptr_t rsdp_phys = virt_to_phys((void*) rsdp_virt);

    pmap_map(pmap_kernel(), rsdp_virt, rsdp_phys, sizeof(acpi_rsdp_t), PROT_READ);

    acpi_rsdp_t* rsdp = (acpi_rsdp_t*)rsdp_virt;

    dprintf("rsdp checksum: %u", rsdp->checksum);

    return 0;
}

// arch_initcall(acpi_init, 0);