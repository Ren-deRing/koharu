#include <koharu/initcall.h>
#include <koharu/kmem.h>
#include <koharu/mmu.h>
#include <koharu/string.h>
#include <koharu/pmap.h>

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
#define PTE_HUGE      (1ULL << 7)
#define PTE_GLOBAL    (1ULL << 8)
#define PTE_NX        (1ULL << 63)

static inline uintptr_t read_cr3(void) {
    uintptr_t phys;
    __asm__ __volatile__(
        "mov %%cr3, %0"
        : "=r" (phys)
    );
    return phys;
}

static inline void write_cr3(uintptr_t phys) {
    __asm__ __volatile__(
        "mov %0, %%cr3"
        :
        : "r" (phys)
        : "memory"
    );
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

    pmap->pm_root_phys = pml4_phys;
    pmap->pm_root_virt = pml4_virt;

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

    // PD -> PT
    idx = PD_INDEX(va);
    if (!(pd[idx] & PTE_PRESENT)) {
        uintptr_t pt_phys = (uintptr_t)pmm_alloc_pages(0);
        if (!pt_phys) return -1;

        memset(phys_to_virt(pt_phys), 0, PAGE_SIZE);
        pd[idx] = pt_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }

    pte_t *pt = (pte_t *)phys_to_virt(pd[idx] & PTE_ADDR_MASK);

    idx = PT_INDEX(va);
    uint64_t pte_flags = PTE_PRESENT;

    // translate flags
    if (flags & PROT_WRITE) {
        pte_flags |= PTE_WRITABLE;
    }
    if (flags & PROT_USER) {
        pte_flags |= PTE_USER;
    }
    if (!(flags & PROT_EXEC)) {
        pte_flags |= PTE_NX;
    }

    pt[idx] = (pa & PTE_ADDR_MASK) | pte_flags;

    return 0;
}

int pmap_map(pmap_t *pmap, uintptr_t virt, uintptr_t phys, size_t size, uint32_t flags) {
    if (!pmap || size == 0) return -1;

    uintptr_t va_start = virt & ~0xFFFULL;                 // 4KB align down (page's start addr)
    uintptr_t va_end = (virt + size + 0xFFF) & ~0xFFFULL;  // 4KB align up   (end boundary addr for loop)
    uintptr_t pa = phys & ~0xFFFULL;                       // 4KB align down (phys page's start addr)

    for (uintptr_t va = va_start; va < va_end; va += PAGE_SIZE, pa += PAGE_SIZE) {
        int err = pmap_map_page(pmap, va, pa, flags);
        if (err != 0) return err;
    }

    return 0;
}

void pmap_destroy(pmap_t *pmap) {
    if (!pmap || pmap == pmap_kernel()) return;

    pml4e_t *pml4 = (pml4e_t *)pmap->pm_root_virt;

    for (int i = 0; i < 256; i++) {
        if (!(pml4[i] & PTE_PRESENT)) continue;

        uintptr_t pdpt_phys = pml4[i] & PTE_ADDR_MASK;
        pdpte_t *pdpt = (pdpte_t *)phys_to_virt(pdpt_phys);

        for (int j = 0; j < 512; j++) {
            if (!(pdpt[j] & PTE_PRESENT) || (pdpt[j] & PTE_HUGE)) continue;

            uintptr_t pd_phys = pdpt[j] & ~0xFFF;
            pde_t *pd = (pde_t *)phys_to_virt(pd_phys);

            for (int k = 0; k < 512; k++) {
                if (!(pd[k] & PTE_PRESENT) || (pd[k] & PTE_HUGE)) continue;

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