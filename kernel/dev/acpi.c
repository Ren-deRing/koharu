#include <koharu/print.h>
#include <koharu/mmu.h>
#include <koharu/bootinfo.h>
#include <koharu/acpi.h>
#include <koharu/initcall.h>
#include <koharu/pmap.h>

int acpi_init() {
    uintptr_t rsdp_virt = g_boot_info->rsdp_addr;
    uintptr_t rsdp_phys = virt_to_phys((void*) rsdp_virt);

    pmap_map(pmap_kernel(), rsdp_virt, rsdp_phys, sizeof(acpi_rsdp_t), PROT_READ | PROT_GLOBAL);

    acpi_rsdp_t* rsdp = (acpi_rsdp_t*)rsdp_virt;

    uint8_t sum = 0;
    for (size_t i = 0; i < sizeof(acpi_rsdp_t); i++) {
        sum += ((uint8_t *)rsdp)[i];
    }

    if (sum != 0) return -1;

    return 0;
}

arch_initcall(acpi_init, 0);