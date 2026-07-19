// Let's Enable HHDM Paging!
// But How?

// Hmm.... page table..

// PML4T -> PDPTT -> PDT -> PT
// HHDM Offset: 0xFFFF800000000000
// 0xFFFF800000000000 -> phys mem
// alright, let's do it

// oh wait, i need memset.
// ok memset ready

#include <stdint.h>

#define EARLY_PAGE_DEFAULT_ADDR      0x80000
#define EARLY_PDPT_ADDR              0x81000
#define EARLY_PDT_ADDR               0x82000
#define PT_ADDR_MASK         0xFFFFFFFFFF000

#define PAGE_DEFAULT_ADDR_PHYS      0x100000
#define PAGE_DEFAULT_ADDR 0xFFFF800000100000

#define PT_PRESENT  (1ULL << 0)
#define PT_WRITABLE (1ULL << 1)
#define PT_HUGE     (1ULL << 7)

#include "string.h"

void init_hhdm(uint64_t mem_size, uint64_t vbe_lfb_end) {
    memset((void *)EARLY_PAGE_DEFAULT_ADDR, 0, 0x6000); // clear page table area.

    uint64_t *early_pml4t = (uint64_t *)EARLY_PAGE_DEFAULT_ADDR;
    uint64_t *early_pdpt = (uint64_t *)EARLY_PDPT_ADDR;
    uint64_t *early_pdt = (uint64_t *)EARLY_PDT_ADDR;

    uint64_t *early_old_pml4t = (uint64_t *)0x1000;
    early_pml4t[0] = early_old_pml4t[0]; // i don't want to die (this bootloader is running on 0x8000 area,
                                         // which is covered by old page tables.)

    early_pml4t[256] = (PT_ADDR_MASK & EARLY_PDPT_ADDR) | (PT_PRESENT | PT_WRITABLE); // 0xFFFF800000000000 is covered by PML4T's 256th child

    for (int i = 0; i < 4; i++) { // we need 4 PDTE for 4GiB
        early_pdpt[i] = (PT_ADDR_MASK & (EARLY_PDT_ADDR + (0x1000 * i))) | (PT_PRESENT | PT_WRITABLE);
    }                              // for each loop: 0x81000 += 0x1000

    for (int i = 0; i < 2048; i++) {
        early_pdt[i] = (PT_ADDR_MASK & (0x200000 * i)) | (PT_PRESENT | PT_WRITABLE | PT_HUGE);
    }                             // similar! but it's phys addr. HHDM basically
                                  // maps physical address directly to the higher address, simply shift them up by 2MiB. (Huge Page)

    asm __volatile__ (
        "movq %0, %%cr3"
        :
        : "r" ((uint64_t)EARLY_PAGE_DEFAULT_ADDR)
        : "memory"
    );

    // next, map entire memory.

    uint64_t max_phys_addr = (mem_size > vbe_lfb_end) ? mem_size : vbe_lfb_end;;

    uint64_t total_pdes = (max_phys_addr + 0x1FFFFFULL) >> 21; // max_phys_addr / 2MiB
    uint64_t total_pdts = (total_pdes + 511ULL) >> 9;          // total_pdes / 512
    uint64_t total_pdpts = (total_pdts + 511ULL) >> 9;         // total_pdts / 512

    uint64_t pdpt_base_phys = PAGE_DEFAULT_ADDR_PHYS + 0x1000ULL;
    uint64_t pdt_base_phys  = pdpt_base_phys + (total_pdpts * 0x1000ULL);

    uint64_t pdpt_base_virt = PAGE_DEFAULT_ADDR + 0x1000ULL;
    uint64_t pdt_base_virt  = pdpt_base_virt + (total_pdpts * 0x1000ULL);

    uint64_t *pml4t = (uint64_t *)PAGE_DEFAULT_ADDR;
    uint64_t *pdpt = (uint64_t *)pdpt_base_virt;

    memset((void *)PAGE_DEFAULT_ADDR, 0, (1 + total_pdpts + total_pdts) * 4096); // clear page table area.

    uint64_t *old_pml4t = (uint64_t *)0x1000;
    pml4t[0] = old_pml4t[0];

    for (uint64_t i = 0; i < total_pdpts; i++) { // we already calculated!
        pml4t[256+i] = (PT_ADDR_MASK & (pdpt_base_phys + (0x1000ULL * i))) | (PT_PRESENT | PT_WRITABLE);
    }

    for (uint64_t i = 0; i < total_pdts; i++) {
        pdpt[i] = (PT_ADDR_MASK & (pdt_base_phys + (0x1000ULL * i))) | (PT_PRESENT | PT_WRITABLE);
    }

    for (uint64_t i = 0; i < total_pdes; i++) {
        uint64_t pdt_index = i / 512;
        uint64_t entry_index = i % 512;

        uint64_t *curr_pdt = (uint64_t *)(pdt_base_virt + (pdt_index * 0x1000ULL));
        curr_pdt[entry_index] = (PT_ADDR_MASK & (0x200000ULL * i)) | (PT_PRESENT | PT_WRITABLE | PT_HUGE);
    }

    asm __volatile__ (
        "movq %0, %%cr3"
        :
        : "r" ((uint64_t)PAGE_DEFAULT_ADDR_PHYS)
        : "memory"
    );
}