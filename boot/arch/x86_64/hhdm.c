#include <bootinfo.h>
#include <uefi.h>

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE   (1ULL << 1)
#define PAGE_HUGE    (1ULL << 7)

static void* alloc_page() {
    efi_physical_address_t addr = 0xFFFFFFFF;
    efi_status_t status = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, 1, &addr);
    if (EFI_ERROR(status)) {
        return NULL;
    }
    memset((void*)addr, 0, 4096);
    return (void*)addr;
}

efi_status_t init_hhdm(uint64_t max_phys_addr, uint64_t image_base, size_t image_size, uint64_t *out_root_phys) {
    uint64_t *pml4 = (uint64_t*)alloc_page();
    if (!pml4) return EFI_OUT_OF_RESOURCES;

    // identity mapping (bootloader)

    for (uint64_t addr = image_base & ~0x1FFFFFULL;
         addr < image_base + image_size;
         addr += 2 * 1024 * 1024) {
        uint64_t pml4_idx = (addr >> 39) & 0x1FF;
        uint64_t pdpt_idx = (addr >> 30) & 0x1FF;
        uint64_t pd_idx   = (addr >> 21) & 0x1FF;

        uint64_t *pdpt;
        if (pml4[pml4_idx] & PAGE_PRESENT) {
            pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFFULL);
        } else {
            pdpt = (uint64_t*)alloc_page();
            if (!pdpt) return EFI_OUT_OF_RESOURCES;
            pml4[pml4_idx] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_WRITE;
        }

        uint64_t *pd;
        if (pdpt[pdpt_idx] & PAGE_PRESENT) {
            pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFFULL);
        } else {
            pd = (uint64_t*)alloc_page();
            if (!pd) return EFI_OUT_OF_RESOURCES;
            pdpt[pdpt_idx] = (uint64_t)pd | PAGE_PRESENT | PAGE_WRITE;
        }

        pd[pd_idx] = addr | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE;
    }

    // HHDM mapping

    uint64_t total_pdes  = (max_phys_addr + 0x1FFFFFULL) >> 21; // max_phys_addr / 2MiB
    uint64_t total_pdts  = (total_pdes + 511ULL) >> 9;          // total_pdes / 512
    uint64_t total_pdpts = (total_pdts + 511ULL) >> 9;          // total_pdts / 512

    uint64_t pml4_base_index = 256; // HHDM region starts at PML4[256]

    uint64_t allocated_pdts  = 0;
    uint64_t allocated_pdes  = 0;

    for (uint64_t i = 0; i < total_pdpts && (pml4_base_index + i) < 512; i++) {
        uint64_t *pdpt = (uint64_t*)alloc_page();
        if (!pdpt) return EFI_OUT_OF_RESOURCES;

        // New PDPT
        pml4[pml4_base_index + i] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_WRITE;

        // Fills PDT
        for (uint64_t j = 0; j < 512; j++) {
            if (allocated_pdts >= total_pdts) {
                break; // Done!
            }

            uint64_t *pd = (uint64_t*)alloc_page();
            if (!pd) return EFI_OUT_OF_RESOURCES;

            pdpt[j] = (uint64_t)pd | PAGE_PRESENT | PAGE_WRITE;
            allocated_pdts++;

            // Fills PDE (2MB)
            for (uint64_t k = 0; k < 512; k++) {
                if (allocated_pdes >= total_pdes) {
                    break; // Done!
                }

                uint64_t phys_addr = allocated_pdes * 2 * 1024 * 1024; // 2MB
                pd[k] = phys_addr | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE;

                allocated_pdes++;
            }
        }
    }

    // kernel mapping

    uint64_t *kernel_pdpt = (uint64_t*)alloc_page();
    if (!kernel_pdpt) return EFI_OUT_OF_RESOURCES;

    pml4[511] = (uint64_t)kernel_pdpt | PAGE_PRESENT | PAGE_WRITE;

    for (uint64_t pdpt_idx = 510; pdpt_idx <= 511; pdpt_idx++) {
        uint64_t *kernel_pd = (uint64_t*)alloc_page();
        if (!kernel_pd) return EFI_OUT_OF_RESOURCES;

        kernel_pdpt[pdpt_idx] = (uint64_t)kernel_pd | PAGE_PRESENT | PAGE_WRITE;

        for (uint64_t pde_idx = 0; pde_idx < 512; pde_idx++) {
            uint64_t phys_addr = ((pdpt_idx - 510) * 1024 * 1024 * 1024ULL) + (pde_idx * 2 * 1024 * 1024ULL);
            kernel_pd[pde_idx] = phys_addr | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE;
        }
    }

    *out_root_phys = (uint64_t)pml4;
    return EFI_SUCCESS;
}