#include <koharu/bootinfo.h>
#include <koharu/grant.h>
#include <koharu/initcall.h>
#include <koharu/kmem.h>
#include <koharu/mmu.h>

#include <stdint.h>
#include <string.h>

static struct pmap *g_root_pmap = NULL;
static uint64_t g_frames        = 0;
static uint16_t *g_refs         = NULL;
static uint16_t *g_maps         = NULL;
static uint8_t *g_pool          = NULL; // phys
static uint8_t *g_pool_v        = NULL; // virt

static int alloc_order(size_t bytes) {
    int order = 0;
    size_t pages = (bytes + PAGE_SIZE - 1) >> PAGE_SHIFT;

    while ((1ULL << order) < pages) order++;
    return order;
}

void grant_set_root(struct pmap *pmap) {
    g_root_pmap = pmap; // rootserver's
}

int grant_pool_build(void) {
    size_t bytes = (g_frames + 7) / 8;

    void *mem = pmm_alloc_pages(alloc_order(bytes));
    if (!mem) return -1;

    memset(phys_to_virt((uintptr_t)mem), 0, (1ULL << (alloc_order(bytes) + PAGE_SHIFT)));

    g_pool = (uint8_t *)mem;
    g_pool_v = (uint8_t *)phys_to_virt((uintptr_t)mem);

    for (uint64_t pfn = 0; pfn < g_frames; pfn++) {
        if (pmm_frame_in_pool(pfn))
            g_pool_v[pfn / 8] |= (uint8_t)(1 << (pfn % 8));
    }

    return 0;
}

static int grant_pool_test(uint64_t pfn) {
    if (pfn >= g_frames) return 0;
    return (g_pool_v[pfn / 8] >> (pfn % 8)) & 1;
}

uintptr_t grant_pool_phys(void) {
    return (uintptr_t)g_pool;
}

size_t grant_pool_bytes(void) {
    return (g_frames + 7) / 8;
}

uint64_t grant_frame_count(void) {
    return g_frames;
}

int grant_holds(struct pmap *pmap, uint64_t pfn, uint8_t rights) {
    if (!pmap || pfn >= g_frames) return 0;

    if (pmap == g_root_pmap) { // wow hello owner
        if (grant_pool_test(pfn) && g_refs[pfn] == 0)
            return 1;
    }

    list_head *head = (struct list_head *)&pmap->grant_list;
    for (list_node *n = head->next; n != (list_node *)head; n = n->next) {
        struct grant_entry *e = container_of(n, struct grant_entry, node);
        if (e->pfn == pfn) // do you have rights?
            return (e->rights & rights) == rights;
    }

    return 0;
}

int grant_add(struct pmap *pmap, uint64_t pfn, uint8_t rights) {
    if (!pmap || pfn >= g_frames) return -1;

    list_head *head = (struct list_head *)&pmap->grant_list;

    for (list_node *n = head->next; n != (list_node *)head; n = n->next) {
        struct grant_entry *e = container_of(n, struct grant_entry, node);
        if (e->pfn == pfn) {
            e->rights |= rights;
            return 0;
        }
    }

    struct grant_entry *e = (struct grant_entry *)kmalloc(sizeof(struct grant_entry));
    if (!e) return -1; // ?

    e->pfn = pfn;
    e->rights = rights;
    list_add_tail(&e->node, head);
    g_refs[pfn]++;

    return 0;
}

int grant_drop(struct pmap *pmap, uint64_t pfn) {
    if (!pmap || pfn >= g_frames) return -1;

    list_head *head = (struct list_head *)&pmap->grant_list;

    for (list_node *n = head->next; n != (list_node *)head; n = n->next) {
        struct grant_entry *e = container_of(n, struct grant_entry, node);
        if (e->pfn == pfn) {
            list_del(&e->node);
            kfree(e);
            if (g_refs[pfn]) g_refs[pfn]--;
            return 0;
        }
    }

    return -1;
}

void grant_mapping_add(uint64_t pfn) {
    if (pfn < g_frames) g_maps[pfn]++;
}

void grant_mapping_remove(uint64_t pfn) {
    if (pfn < g_frames && g_maps[pfn]) g_maps[pfn]--;
}

static int grant_init(void) {
    g_frames = (g_boot_info->memory.max_phys_addr >> PAGE_SHIFT) + 1;

    g_refs = (uint16_t *)kmalloc(g_frames * sizeof(uint16_t));
    if (!g_refs) return -1;
    g_maps = (uint16_t *)kmalloc(g_frames * sizeof(uint16_t));
    if (!g_maps) return -1;

    memset(g_refs, 0, g_frames * sizeof(uint16_t));
    memset(g_maps, 0, g_frames * sizeof(uint16_t));

    return 0;
}

sys_initcall(grant_init, 0);
