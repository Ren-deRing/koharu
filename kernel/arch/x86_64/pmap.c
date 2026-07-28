#include <koharu/kmem.h>
#include <koharu/mmu.h>
#include <koharu/string.h>

#include <asm/pmap.h>

static pmap_t g_kernel_pmap;

pmap_t* pmap_kernel(void) {
    return &g_kernel_pmap;
}

pmap_t* pmap_create(void) {
    pmap_t *pmap = kmalloc(sizeof(pmap_t));
    if (!pmap) return NULL;

    uintptr_t pml4_phys = (uintptr_t)pmm_alloc_pages(0);
    pml4e_t *pml4_virt = (pml4e_t*)phys_to_virt(pml4_phys);

    memset(pml4_virt, 0, PAGE_SIZE);

    pml4e_t *kernel_pml4 = (pml4e_t *)g_kernel_pmap.pm_root_virt;
    for (int i = 256; i < 512; i++) {
        pml4_virt[i] = kernel_pml4[i];
    } // Dear MMU, Please Copy Table To New PMAP. I've Baked A Cake For You. Yours truly-- Princess kernel, koharu.

    pmap->pm_root_phys = pml4_phys;
    pmap->pm_root_virt = pml4_virt;

    return pmap;
}

void pmap_destroy(pmap_t *pmap) {
    if (!pmap || pmap == pmap_kernel()) return;

    pml4e_t *pml4 = (pml4e_t *)pmap->pm_root_virt;

    for (int i = 0; i < 256; i++) {
        if (!(pml4[i] & PTE_PRESENT)) continue;

        uintptr_t pdpt_phys = pml4[i] & ~0xFFF;
        pdpte_t *pdpt = (pdpte_t *)phys_to_virt(pdpt_phys);

        for (int j = 0; j < 512; j++) {
            if (!(pdpt[j] & PTE_PRESENT) || (pdpt[j] & PTE_HUGE)) continue;

            uintptr_t pd_phys = pdpt[j] & ~0xFFF;
            pde_t *pd = (pde_t *)phys_to_virt(pd_phys);

            for (int k = 0; k < 512; k++) {
                if (!(pd[k] & PTE_PRESENT) || (pd[k] & PTE_HUGE)) continue;

                uintptr_t pt_phys = pd[k] & ~0xFFF;
                pmm_free_pages((void *)pt_phys, 0); // PT
            }
            pmm_free_pages((void *)pd_phys, 0); // PD
        }
        pmm_free_pages((void *)pdpt_phys, 0); // PDPT
    }

    pmm_free_pages((void *)pmap->pm_root_phys, 0);
    kfree(pmap);
}