#pragma once

#include <stddef.h>
#include <stdint.h>

typedef uint64_t pml4e_t;
typedef uint64_t pdpte_t;
typedef uint64_t pde_t;
typedef uint64_t pte_t;

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

typedef struct pmap {
    void *pm_root_virt;
    uintptr_t pm_root_phys;
} pmap_t;

pmap_t* pmap_kernel(void);
pmap_t* pmap_create(void);
void pmap_destroy(pmap_t *pmap);

int pmap_map(pmap_t *pmap, uintptr_t virt, uintptr_t phys, size_t size, uint32_t flags);
void pmap_unmap(pmap_t *pmap, uintptr_t virt, size_t size);

void pmap_activate(pmap_t *pmap);
uintptr_t pmap_extract(pmap_t *pmap, uintptr_t virt);