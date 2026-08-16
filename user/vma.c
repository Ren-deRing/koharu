#include <vma.h>

#include <console.h>
#include <root.h>

#include <stddef.h>

#define VMA_POOL_MAX 256

static struct vma vma_pool[VMA_POOL_MAX];
static struct vma *vma_free;

static struct vma *vma_alloc(void) {
    struct vma *v = vma_free;
    if (v != NULL) vma_free = v->next;
    return v;
}

static void vma_release(struct vma *v) {
    v->next = vma_free;
    vma_free = v;
}

static int vma_cmp(struct vma *a, struct vma *b) {
    if (a->start < b->start) return -1;
    if (a->start > b->start) return 1;
    return 0;
}

RB_GENERATE(vma_tree, vma, rb, vma_cmp)

void vma_set_init(struct vma_set *set) {
    RB_INIT(&set->tree);
    set->first = NULL;
    set->last = NULL;

    if (vma_free == NULL) {
        for (size_t i = 0; i < VMA_POOL_MAX - 1; i++)
            vma_pool[i].next = &vma_pool[i + 1];
        vma_pool[VMA_POOL_MAX - 1].next = NULL;
        vma_free = vma_pool;
    }
}

int vma_insert(struct vma_set *set, uint64_t start, uint64_t end, uint64_t prot, uint64_t kind) {
    struct vma key = { .start = start, .end = end };
    struct vma *prev = NULL;
    struct vma *next = vma_tree_RB_NFIND(&set->tree, &key);
    if (next != NULL) prev = vma_tree_RB_PREV(next);
    else prev = vma_tree_RB_MINMAX(&set->tree, 1);

    if (prev != NULL && prev->end > start) return -1;
    if (next != NULL && next->start < end) return -1;

    struct vma *v = vma_alloc();
    if (v == NULL) return -1;

    v->start = start;
    v->end = end;
    v->prot = prot;
    v->kind = kind;
    v->prev = prev;
    v->next = next;

    if (prev != NULL) prev->next = v;
    else set->first = v;
    if (next != NULL) next->prev = v;
    else set->last = v;

    vma_tree_RB_INSERT(&set->tree, v);
    return 0;
}

struct vma *vma_find(struct vma_set *set, uint64_t addr) {
    struct vma key = { .start = addr, .end = addr };
    struct vma *v = vma_tree_RB_NFIND(&set->tree, &key);
    if (v == NULL) v = vma_tree_RB_MINMAX(&set->tree, 1);
    if (v != NULL && v->start <= addr && addr < v->end) return v;

    struct vma *prev = vma_tree_RB_PREV(v);
    if (prev != NULL && prev->start <= addr && addr < prev->end) return prev;
    return NULL;
}

void vma_remove(struct vma_set *set, struct vma *v) {
    if (v->prev != NULL) v->prev->next = v->next;
    else set->first = v->next;
    if (v->next != NULL) v->next->prev = v->prev;
    else set->last = v->prev;

    vma_tree_RB_REMOVE(&set->tree, v);
    vma_release(v);
}

void vma_dump(struct vma_set *set) {
    for (struct vma *v = set->first; v != NULL; v = v->next) {
        log_str("[vma] ");
        log_hex(v->start);
        log_str(" - ");
        log_hex(v->end);
        log_str(" prot=");
        log_ulong(v->prot);
        log_str(" kind=");
        log_ulong(v->kind);
        log_str("\n");
    }
}
