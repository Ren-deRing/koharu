#pragma once

#include <tree.h>

#include <stdint.h>

#define VMA_CODE  1
#define VMA_DATA  2
#define VMA_STACK 3
#define VMA_HEAP  4

struct vma {
    uint64_t start;
    uint64_t end;
    uint64_t prot;
    uint64_t kind;
    struct vma *prev;
    struct vma *next;
    RB_ENTRY(vma) rb;
};

RB_HEAD(vma_tree, vma);
RB_PROTOTYPE(vma_tree, vma, rb, vma_cmp);

struct vma_set {
    struct vma_tree tree;
    struct vma *first;
    struct vma *last;
};

void vma_set_init(struct vma_set *set);
int vma_insert(struct vma_set *set, uint64_t start, uint64_t end, uint64_t prot, uint64_t kind);
struct vma *vma_find(struct vma_set *set, uint64_t addr);
void vma_remove(struct vma_set *set, struct vma *vma);
void vma_dump(struct vma_set *set);
