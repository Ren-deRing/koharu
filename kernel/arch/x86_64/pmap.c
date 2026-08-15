#include <koharu/initcall.h>
#include <koharu/kmem.h>
#include <koharu/mmu.h>
#include <koharu/string.h>
#include <koharu/pmap.h>

#include <stdbool.h>

typedef uint64_t pml4e_t;
typedef uint64_t pdpte_t;
typedef uint64_t pde_t;
typedef uint64_t pte_t;

#define PML4_INDEX(va) (((va) >> 39) & 0x1FF)
#define PDPT_INDEX(va) (((va) >> 30) & 0x1FF)
#define PD_INDEX(va)   (((va) >> 21) & 0x1FF)
#define PT_INDEX(va)   (((va) >> 12) & 0x1FF)

#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define PTE_PRESENT   (1ULL << 0)
#define PTE_WRITABLE  (1ULL << 1)
#define PTE_USER      (1ULL << 2)
#define PTE_PWT       (1ULL << 3)
#define PTE_PCD       (1ULL << 4)
#define PTE_ACCESSED  (1ULL << 5)
#define PTE_DIRTY     (1ULL << 6)
#define PTE_PAT       (1ULL << 7)
#define PTE_GLOBAL    (1ULL << 8)
#define PTE_NX        (1ULL << 63)

// for PDE / PDPTE
#define PDE_HUGE      (1ULL << 7)
#define PDE_PAT       (1ULL << 12)

static inline uintptr_t read_cr3(void) {
    uintptr_t phys;
    asm __volatile__(
        "mov %%cr3, %0"
        : "=r" (phys)
    );
    return phys;
}

static inline void write_cr3(uintptr_t phys) {
    asm __volatile__(
        "mov %0, %%cr3"
        :
        : "r" (phys)
        : "memory"
    );
}

static inline void flush_tlb(uintptr_t virt) {
    asm __volatile__("invlpg (%0)" ::"r"(virt) : "memory");
}

static void flush_tlb_all(void) {
    uintptr_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    asm volatile("mov %0, %%cr4" :: "r"(cr4 & ~(1ULL << 7)) : "memory");
    asm volatile("mov %0, %%cr3" :: "r"(read_cr3()) : "memory");
    asm volatile("mov %0, %%cr4" :: "r"(cr4) : "memory");
}

static bool is_table_empty(uint64_t *table) {
    for (int i = 0; i < 512; i++) {
        if (table[i] & PTE_PRESENT) return false;
    }
    return true;
}

static pmap_t g_kernel_pmap;

pmap_t* pmap_kernel(void) {
    return &g_kernel_pmap;
}

pmap_t* pmap_create(void) {
    pmap_t *pmap = kmalloc(sizeof(pmap_t)); // allocate new pmap
    if (!pmap) return NULL;

    uintptr_t pml4_phys = (uintptr_t)pmm_alloc_pages(0); // allocate new phys page
    pml4e_t *pml4_virt = (pml4e_t*)phys_to_virt(pml4_phys);

    memset(pml4_virt, 0, PAGE_SIZE); // clean the page
    memcpy(&pml4_virt[256], &((pml4e_t*)g_kernel_pmap.pm_root_virt)[256], 256 * sizeof(pml4e_t));

    pmap->pm_root_phys = pml4_phys;
    pmap->pm_root_virt = pml4_virt;
    
    list_init((struct list_head *)&pmap->grant_list);

    return pmap;
}

static int pmap_map_page(pmap_t *pmap, uintptr_t va, uintptr_t pa, uint32_t flags) {
    pml4e_t *pml4 = (pml4e_t *)pmap->pm_root_virt;

    // PML4 -> PDPT
    size_t idx = PML4_INDEX(va);
    if (!(pml4[idx] & PTE_PRESENT)) {
        uintptr_t pdpt_phys = (uintptr_t)pmm_alloc_pages(0);
        if (!pdpt_phys) return -1;

        memset(phys_to_virt(pdpt_phys), 0, PAGE_SIZE);
        pml4[idx] = pdpt_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }

    pdpte_t *pdpt = (pdpte_t *)phys_to_virt(pml4[idx] & PTE_ADDR_MASK);

    // PDPT -> PD
    idx = PDPT_INDEX(va);
    if (!(pdpt[idx] & PTE_PRESENT)) {
        uintptr_t pd_phys = (uintptr_t)pmm_alloc_pages(0);
        if (!pd_phys) return -1;

        memset(phys_to_virt(pd_phys), 0, PAGE_SIZE);
        pdpt[idx] = pd_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }

    pde_t *pd = (pde_t *)phys_to_virt(pdpt[idx] & PTE_ADDR_MASK);

    // HUGE page
    uint64_t pte_flags = PTE_PRESENT;
    if (flags & PROT_WRITE)   pte_flags |= PTE_WRITABLE;
    if (flags & PROT_USER)    pte_flags |= PTE_USER;
    if (flags & PROT_GLOBAL)  pte_flags |= PTE_GLOBAL;
    if (!(flags & PROT_EXEC)) pte_flags |= PTE_NX;

    if (flags & PROT_HUGE) {
        idx = PD_INDEX(va);

        if ((pd[idx] & PTE_PRESENT) && !(pd[idx] & PDE_HUGE)) {
            pmm_free_pages((void *)(pd[idx] & PTE_ADDR_MASK), 0);
        }

        pte_flags |= PDE_HUGE;
        if (flags & PROT_WC) pte_flags |= PDE_PAT;

        uint64_t pd_addr_mask = 0x000FFFFFFFE00000ULL; 
        pd[idx] = (pa & pd_addr_mask) | pte_flags;

        return 0; // no pt
    }

    // normal page
    idx = PD_INDEX(va);
    if (!(pd[idx] & PTE_PRESENT)) {
        uintptr_t pt_phys = (uintptr_t)pmm_alloc_pages(0);
        if (!pt_phys) return -1;

        memset(phys_to_virt(pt_phys), 0, PAGE_SIZE);
        pd[idx] = pt_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }

    pte_t *pt = (pte_t *)phys_to_virt(pd[idx] & PTE_ADDR_MASK);

    idx = PT_INDEX(va);
    if (flags & PROT_WC) pte_flags |= PTE_PAT;

    pt[idx] = (pa & PTE_ADDR_MASK) | pte_flags;

    return 0;
}

static size_t pmap_unmap_page(pmap_t *pmap, uintptr_t va, bool *freed) {
    pml4e_t *pml4 = (pml4e_t *)pmap->pm_root_virt;

    // PML4 -> PDPT
    size_t l4 = PML4_INDEX(va);
    if (!(pml4[l4] & PTE_PRESENT)) return PAGE_SIZE;
    pdpte_t *pdpt = (pdpte_t *)phys_to_virt(pml4[l4] & PTE_ADDR_MASK);

    // PDPT -> PD
    size_t l3 = PDPT_INDEX(va);
    if (!(pdpt[l3] & PTE_PRESENT) || (pdpt[l3] & PDE_HUGE)) return PAGE_SIZE;
    pde_t *pd = (pde_t *)phys_to_virt(pdpt[l3] & PTE_ADDR_MASK);

    // HUGE page
    size_t l2 = PD_INDEX(va);
    if (!(pd[l2] & PTE_PRESENT)) return PAGE_SIZE;

    if (pd[l2] & PDE_HUGE) {
        pd[l2] = 0;

        if (read_cr3() == pmap->pm_root_phys) {
            flush_tlb(va);
        }
        return PAGE_SIZE_HUGE;
    }

    // normal page
    pte_t *pt = (pte_t *)phys_to_virt(pd[l2] & PTE_ADDR_MASK);

    size_t l1 = PT_INDEX(va);
    if (!(pt[l1] & PTE_PRESENT)) return PAGE_SIZE;

    pt[l1] = 0;

    if (read_cr3() == pmap->pm_root_phys) {
        flush_tlb(va);
    }

    // teardown
    if (is_table_empty(pt)) {
        pmm_free_pages((void *)(pd[l2] & PTE_ADDR_MASK), 0); // PT
        pd[l2] = 0;

        if (is_table_empty(pd)) {
            pmm_free_pages((void *)(pdpt[l3] & PTE_ADDR_MASK), 0); // PD
            pdpt[l3] = 0;

            if (is_table_empty(pdpt)) {
                pmm_free_pages((void *)(pml4[l4] & PTE_ADDR_MASK), 0); // PDPT
                pml4[l4] = 0;
            }
        }

        *freed = true;
    }

    return PAGE_SIZE;
}

int pmap_map(pmap_t *pmap, uintptr_t virt, uintptr_t phys, size_t size, uint32_t flags) {
    if (!pmap || size == 0) return -1;

    size_t step = (flags & PROT_HUGE) ? PAGE_SIZE_HUGE : PAGE_SIZE;
    uintptr_t mask = step - 1;

    uintptr_t va_start = virt & ~mask;                 // 4KB align down (page's start addr)
    uintptr_t va_end = (virt + size + mask) & ~mask;   // 4KB align up   (end boundary addr for loop)
    uintptr_t pa = phys & ~mask;                       // 4KB align down (phys page's start addr)                     

    for (uintptr_t va = va_start; va < va_end; va += step, pa += step) {
        int err = pmap_map_page(pmap, va, pa, flags);   
        if (err != 0) return err;
    }

    if (read_cr3() == pmap->pm_root_phys) {
        flush_tlb_all();
    }

    return 0;
}

void pmap_unmap(pmap_t *pmap, uintptr_t virt, size_t size) {
    if (!pmap || size == 0) return;

    uintptr_t va_start = virt & ~0xFFFULL;
    uintptr_t va_end   = (virt + size + 0xFFF) & ~0xFFFULL;

    bool freed = false;

    for (uintptr_t va = va_start; va < va_end;) {
        va += pmap_unmap_page(pmap, va, &freed);
    }

    if (freed && read_cr3() == pmap->pm_root_phys) {
        flush_tlb_all();
    }
}

uintptr_t pmap_extract(pmap_t *pmap, uintptr_t va) {
    if (!pmap) return 0;

    pml4e_t *pml4 = (pml4e_t *)pmap->pm_root_virt;

    // PML4 -> PDPT
    size_t l4 = PML4_INDEX(va);
    if (!(pml4[l4] & PTE_PRESENT)) return 0;
    pdpte_t *pdpt = (pdpte_t *)phys_to_virt(pml4[l4] & PTE_ADDR_MASK);

    // PDPT -> PD
    size_t l3 = PDPT_INDEX(va);
    if (!(pdpt[l3] & PTE_PRESENT)) return 0;
    pde_t *pd = (pde_t *)phys_to_virt(pdpt[l3] & PTE_ADDR_MASK);

    // HUGE page
    size_t l2 = PD_INDEX(va);
    if (!(pd[l2] & PTE_PRESENT)) return 0;

    if (pd[l2] & PDE_HUGE) {
        return (pd[l2] & 0x000FFFFFFFE00000ULL) + (va & 0x1FFFFF);
    }

    // normal page
    pte_t *pt = (pte_t *)phys_to_virt(pd[l2] & PTE_ADDR_MASK);

    size_t l1 = PT_INDEX(va);
    if (!(pt[l1] & PTE_PRESENT)) return 0;

    return (pt[l1] & PTE_ADDR_MASK) + (va & 0xFFF);
}

void pmap_destroy(pmap_t *pmap) {
    if (!pmap || pmap == pmap_kernel()) return;

    pml4e_t *pml4 = (pml4e_t *)pmap->pm_root_virt;

    for (int i = 0; i < 256; i++) {
        if (!(pml4[i] & PTE_PRESENT)) continue;

        uintptr_t pdpt_phys = pml4[i] & PTE_ADDR_MASK;
        pdpte_t *pdpt = (pdpte_t *)phys_to_virt(pdpt_phys);

        for (int j = 0; j < 512; j++) {
            if (!(pdpt[j] & PTE_PRESENT) || (pdpt[j] & PDE_HUGE)) continue;

            uintptr_t pd_phys = pdpt[j] & ~0xFFF;
            pde_t *pd = (pde_t *)phys_to_virt(pd_phys);

            for (int k = 0; k < 512; k++) {
                if (!(pd[k] & PTE_PRESENT) || (pd[k] & PDE_HUGE)) continue;

                uintptr_t pt_phys = pd[k] & PTE_ADDR_MASK;
                pmm_free_pages((void *)pt_phys, 0); // PT
            }
            pmm_free_pages((void *)pd_phys, 0); // PD
        }
        pmm_free_pages((void *)pdpt_phys, 0); // PDPT
    }

    pmm_free_pages((void *)pmap->pm_root_phys, 0);
    kfree(pmap);
}

void pmap_activate(pmap_t *pmap) {
    if (!pmap || pmap->pm_root_phys == 0) {
        return;
    }

    write_cr3(pmap->pm_root_phys);
}

int pmap_init(void) {
    uintptr_t pml4_phys = (uintptr_t)pmm_alloc_pages(0);
    if (!pml4_phys) return -1;

    pml4e_t *pml4_virt = (pml4e_t*)phys_to_virt(pml4_phys);
    memset(pml4_virt, 0, PAGE_SIZE);

    g_kernel_pmap.pm_root_phys = pml4_phys;
    g_kernel_pmap.pm_root_virt = pml4_virt;

    return 0;
}