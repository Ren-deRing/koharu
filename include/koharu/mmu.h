#pragma once

#include <koharu/list.h>

#include <stdint.h>

#define PAGE_SHIFT 12 
#define PAGE_SIZE  (1ULL << PAGE_SHIFT)
#define MAX_BUDDY_ORDER 11

#define ALIGN_UP(addr, align)   (((addr) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(addr, align) ((addr) & ~((align) - 1))

#define HHDM_OFFSET 0xFFFF800000000000ULL

typedef struct {
    list_node page_list;
    bool      is_free;
    uint16_t  owner_cpu_id;
} page_t;

void* pmm_alloc_pages(int order);
void pmm_free_pages(void* addr, int order);

uintptr_t virt_to_phys(void *addr);
void* phys_to_virt(uintptr_t phys);

size_t phys_to_pfn(uintptr_t phys);
uintptr_t pfn_to_phys(size_t pfn);
page_t* pfn_to_page(size_t pfn);
size_t page_to_pfn(page_t* pg);

page_t* virt_to_page(void *ptr);
void* page_to_virt(page_t *page);