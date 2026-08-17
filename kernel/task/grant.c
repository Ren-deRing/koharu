#include <koharu/bootinfo.h>
#include <koharu/grant.h>
#include <koharu/initcall.h>
#include <koharu/kmem.h>
#include <koharu/lock.h>
#include <koharu/mmu.h>

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

static struct pmap *g_root_pmap = NULL;
static uint64_t g_frames        = 0;
static _Atomic uint16_t *g_refs = NULL;
static _Atomic uint16_t *g_maps = NULL;
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

    if (pmap == g_root_pmap) {
        if (grant_pool_test(pfn) && atomic_load(&g_refs[pfn]) == 0)
            return 1;
    }

    uint64_t f = spin_lock_irqsave(&pmap->lock);

    list_head *head = (struct list_head *)&pmap->grant_list;
    for (list_node *n = head->next; n != (list_node *)head; n = n->next) {
        struct grant_entry *e = container_of(n, struct grant_entry, node);
        if (e->pfn == pfn) {
            spin_unlock_irqrestore(&pmap->lock, f);
            return (e->rights & rights) == rights;
        }
    }

    spin_unlock_irqrestore(&pmap->lock, f);
    return 0;
}

int grant_add(struct pmap *pmap, uint64_t pfn, uint8_t rights) {
    if (!pmap || pfn >= g_frames) return -1;

    struct grant_entry *e = (struct grant_entry *)kmalloc(sizeof(struct grant_entry));
    if (!e) return -1;

    e->pfn = pfn;
    e->rights = rights;

    uint64_t f = spin_lock_irqsave(&pmap->lock);

    list_head *head = (struct list_head *)&pmap->grant_list;

    for (list_node *n = head->next; n != (list_node *)head; n = n->next) {
        struct grant_entry *existing = container_of(n, struct grant_entry, node);
        if (existing->pfn == pfn) {
            existing->rights |= rights;
            spin_unlock_irqrestore(&pmap->lock, f);
            kfree(e);
            return 0;
        }
    }

    list_add_tail(&e->node, head);
    atomic_fetch_add(&g_refs[pfn], 1);

    spin_unlock_irqrestore(&pmap->lock, f);
    return 0;
}

int grant_drop(struct pmap *pmap, uint64_t pfn) {
    if (!pmap || pfn >= g_frames) return -1;

    uint64_t f = spin_lock_irqsave(&pmap->lock);

    list_head *head = (struct list_head *)&pmap->grant_list;

    for (list_node *n = head->next; n != (list_node *)head; n = n->next) {
        struct grant_entry *e = container_of(n, struct grant_entry, node);
        if (e->pfn == pfn) {
            list_del(&e->node);
            kfree(e);
            if (atomic_load(&g_refs[pfn]))
                atomic_fetch_sub(&g_refs[pfn], 1);
            spin_unlock_irqrestore(&pmap->lock, f);
            return 0;
        }
    }

    spin_unlock_irqrestore(&pmap->lock, f);
    return -1;
}

void grant_mapping_add(uint64_t pfn) {
    if (pfn < g_frames) atomic_fetch_add(&g_maps[pfn], 1);
}

void grant_mapping_remove(uint64_t pfn) {
    if (pfn < g_frames && atomic_load(&g_maps[pfn]))
        atomic_fetch_sub(&g_maps[pfn], 1);
}

static int grant_init(void) {
    g_frames = (g_boot_info->memory.max_phys_addr >> PAGE_SHIFT) + 1;

    g_refs = (_Atomic uint16_t *)kmalloc(g_frames * sizeof(_Atomic uint16_t));
    if (!g_refs) return -1;
    g_maps = (_Atomic uint16_t *)kmalloc(g_frames * sizeof(_Atomic uint16_t));
    if (!g_maps) return -1;

    memset((void *)g_refs, 0, g_frames * sizeof(_Atomic uint16_t));
    memset((void *)g_maps, 0, g_frames * sizeof(_Atomic uint16_t));

    return 0;
}

sys_initcall(grant_init, 0);
