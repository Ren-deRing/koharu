// Let's Enable HHDM Paging!
// But How?

// Hmm.... page table..

// PML4T -> PDPTT -> PDT -> PT
// HHDM Offset: 0xFFFF800000000000
// 0xFFFF800000000000 -> 0xFFFF800000400000000
// alright, let's do it

// oh wait, i need memset.
// ok memset ready

#include <stdint.h>

#define PAGE_DEFAULT_ADDR    0x80000
#define PDPT_ADDR            0x81000
#define PDT_ADDR             0x82000
#define PT_ADDR_MASK 0xFFFFFFFFFF000

#define PT_PRESENT  (1ULL << 0)
#define PT_WRITABLE (1ULL << 1)
#define PT_HUGE     (1ULL << 7)

#include "string.h"

void init_hhdm(void) {
    memset((void *)PAGE_DEFAULT_ADDR, 0, 0x6000); // clear page table area.

    uint64_t *pml4t = (uint64_t *)PAGE_DEFAULT_ADDR;
    uint64_t *pdpt = (uint64_t *)PDPT_ADDR;
    uint64_t *pdt = (uint64_t *)PDT_ADDR;

    uint64_t *old_pml4t = (uint64_t *)0x1000;
    pml4t[0] = old_pml4t[0]; // i don't want to die (this bootloader is running on 0x8000 area,
                             // which is covered by old page tables.)

    pml4t[256] = (PT_ADDR_MASK & PDPT_ADDR) | (PT_PRESENT | PT_WRITABLE); // 0xFFFF800000000000 is covered by PML4T's 256th child

    for (int i = 0; i < 4; i++) { // we need 4 PDTE for 4GiB
        pdpt[i] = (PT_ADDR_MASK & (PDT_ADDR + (0x1000 * i))) | (PT_PRESENT | PT_WRITABLE);
    }                              // for each loop: 0x81000 += 0x1000

    for (int i = 0; i < 2048; i++) {
        pdt[i] = (PT_ADDR_MASK & (0x200000 * i)) | (PT_PRESENT | PT_WRITABLE | PT_HUGE);
    }                             // similar! but it's phys addr. HHDM basically
                                  // maps physical address directly to the higher address, simply shift them up by 2MiB. (Huge Page)

    asm __volatile__ (
        "movq %0, %%cr3"
        :
        : "r" ((uint64_t)PAGE_DEFAULT_ADDR)
        : "memory"
    );
}