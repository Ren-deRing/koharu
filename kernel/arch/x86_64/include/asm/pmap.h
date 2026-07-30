#pragma once

#include <stddef.h>
#include <stdint.h>

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

int pmap_init(void);