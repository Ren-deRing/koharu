#pragma once

#include <koharu/list.h>
#include <koharu/pmap.h>

#include <stddef.h>
#include <stdint.h>

#define GRANT_READ   (1 << 0)
#define GRANT_WRITE  (1 << 1)
#define GRANT_GRANT  (1 << 2)

struct grant_entry {
    list_node node;
    uint64_t  pfn;
    uint8_t   rights;
};

void grant_set_root(struct pmap *pmap);

int grant_pool_build(void);
uintptr_t grant_pool_phys(void);
size_t grant_pool_bytes(void);
uint64_t grant_frame_count(void);

int grant_holds(struct pmap *pmap, uint64_t pfn, uint8_t rights);
int grant_add(struct pmap *pmap, uint64_t pfn, uint8_t rights);
int grant_drop(struct pmap *pmap, uint64_t pfn);

void grant_mapping_add(uint64_t pfn);
void grant_mapping_remove(uint64_t pfn);
