#include <koharu/bootinfo.h>
#include <koharu/cpu.h>
#include <koharu/mmu.h>
#include <koharu/initcall.h>
#include <koharu/list.h>
#include <koharu/print.h>
#include <koharu/string.h>

#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    list_head free_list;
    size_t    free_count;
} buddy_order;

typedef struct {
    buddy_order orders[MAX_BUDDY_ORDER];
    page_t     *mem_map;
    uintptr_t   mem_map_phys;
    size_t      mem_map_size;
    size_t      total_pages;
    uintptr_t   mem_start;
    uintptr_t   mem_end;
    size_t      free_pages;
} pmm_t;

static pmm_t g_pmm;

uintptr_t virt_to_phys(void *addr) {
    return (uintptr_t)addr - HHDM_OFFSET;
}

void* phys_to_virt(uintptr_t phys) {
    return (void *)(phys + HHDM_OFFSET);
}

size_t phys_to_pfn(uintptr_t phys) {
    return (size_t)(phys >> PAGE_SHIFT);
}

uintptr_t pfn_to_phys(size_t pfn) {
    return (uintptr_t)pfn << PAGE_SHIFT;
}

page_t* pfn_to_page(size_t pfn) {
    return &g_pmm.mem_map[pfn];
}

size_t page_to_pfn(page_t* pg) {
    return (size_t)(pg - g_pmm.mem_map);
}

page_t* virt_to_page(void *ptr) {
    uintptr_t phys = virt_to_phys(ptr);
    size_t pfn  = phys_to_pfn(phys);

    return &g_pmm.mem_map[pfn];
}

void* page_to_virt(page_t *page) {
    uintptr_t pfn  = page_to_pfn(page);
    uintptr_t phys = pfn_to_phys(pfn);

    return phys_to_virt(phys);
}

void* pmm_alloc_pages(int order) {
    if (order < 0 || order >= MAX_BUDDY_ORDER) return NULL;

    for (int curr_order = order; curr_order < MAX_BUDDY_ORDER; curr_order++) {
        if (list_empty(&g_pmm.orders[curr_order].free_list)) continue; // no free block here? continue to higher order
        
        list_node* node = g_pmm.orders[curr_order].free_list.next; // i think this is fine.
        list_del(node);                                            // delete from freelist
        g_pmm.orders[curr_order].free_count--;                     // as you know.

        size_t pfn = page_to_pfn((page_t*)node);

        while (curr_order > order) { // if splited?
            curr_order--;

            size_t buddy_pfn = pfn ^ (1ULL << curr_order); // hey buddy, where are you?

            page_t* buddy = pfn_to_page(buddy_pfn); // oh, there!
            buddy->is_free = true;

            list_add(&buddy->page_list, &g_pmm.orders[curr_order].free_list); // add to freelist..
            g_pmm.orders[curr_order].free_count++;
        }

        page_t* page = pfn_to_page(pfn);
        page->is_free = false;

        g_pmm.free_pages -= (1ULL << order);

        return (void*)pfn_to_phys(pfn); // return phys addr
    }

    return NULL; // OOM!
}

void pmm_free_pages(void* addr, int order) {
    if (addr == NULL || order < 0 || order >= MAX_BUDDY_ORDER) return;

    size_t pfn = phys_to_pfn((uintptr_t)addr);
    int curr_order = order;

    while (curr_order < MAX_BUDDY_ORDER - 1) {
        size_t buddy_pfn = pfn ^ (1ULL << curr_order); // hey ~~~
        page_t* buddy = pfn_to_page(buddy_pfn); // oh, ~~~~

        if (!buddy->is_free) break; // buddy is not free

        list_del(&buddy->page_list);
        buddy->is_free = false;
        g_pmm.orders[curr_order].free_count--;

        pfn = pfn & buddy_pfn; // merge!
        curr_order++;
    }

    // add merged page to list
    page_t* page = pfn_to_page(pfn);
    page->is_free = true;
    list_add_tail(&page->page_list, &g_pmm.orders[curr_order].free_list);
    g_pmm.orders[curr_order].free_count++;

    g_pmm.free_pages += (1ULL << order);
}

// int pmm_init() {
//     g_pmm.free_pages = 0;
    
//     for (int i = 0; i < MAX_BUDDY_ORDER; i++) {
//         g_pmm.orders[i].free_count = 0;
//         list_init(&g_pmm.orders[i].free_list);
//     }

//     // mem map size....
//     g_pmm.total_pages = ALIGN_UP(g_boot_info->memory.max_phys_addr, PAGE_SIZE) / PAGE_SIZE;
//     size_t array_size = ALIGN_UP(g_pmm.total_pages * sizeof(page_t), PAGE_SIZE);

//     // mem map is located in a free region of ​​sufficient size
//     mmap_entry_t *mmap = g_boot_info->memory.entries;
//     uint32_t count = g_boot_info->memory.count;
//     uintptr_t array_phys = 0;

//     for (size_t i = 0; i < count; i++) {
//         if (mmap[i].type == MMAP_TYPE_USABLE) {
//             uintptr_t start_addr = ALIGN_UP(mmap[i].phys_start, PAGE_SIZE);
//             uintptr_t end_addr = ALIGN_UP(mmap[i].phys_start + mmap[i].length, PAGE_SIZE);

//             if (start_addr < 0x100000ULL) {
//                 start_addr = 0x100000ULL;
//             }

//             if (end_addr > start_addr && (end_addr - start_addr) >= array_size) {
//                 array_phys = start_addr;
//                 break;
//             }
//         }
//     }

//     if (!array_phys || array_phys == 0) return -1; // oh no...

//     g_pmm.mem_map_phys = array_phys;
//     g_pmm.mem_map_size = array_size;
//     g_pmm.mem_map = (page_t*)(array_phys + HHDM_OFFSET);

//     memset(g_pmm.mem_map, 0, array_size);

//     uintptr_t array_end = array_phys + array_size;

//     for (uint64_t i = 0; i < count; i++) {
//         if (mmap[i].type == MMAP_TYPE_USABLE) {
//             uintptr_t curr = ALIGN_UP(mmap[i].base_addr, PAGE_SIZE);
//             uintptr_t end = ALIGN_DOWN(mmap[i].base_addr + mmap[i].length, PAGE_SIZE);

//             while (curr < end) {
//                 if (curr >= array_phys && curr < array_end) { // if curr is in a mem map array
//                     curr = array_end;
//                     continue;
//                 }

//                 uintptr_t limit = end;
//                 if (curr < array_phys && end > array_phys) limit = array_phys; // don't overlap me..

//                 size_t remain = limit - curr;
//                 int target_order = 0; // zerokara

//                 for (int order = MAX_BUDDY_ORDER - 1; order >= 0; order--) {
//                     size_t block_size = 1ULL << (PAGE_SHIFT + order);
//                     if (remain >= block_size && (curr & (block_size - 1)) == 0) {
//                         target_order = order; // if addr is aligend to block
//                         break;                // and the size is smaller than block size
//                     }
//                 }

//                 size_t pfn = phys_to_pfn(curr);
//                 page_t* pg = pfn_to_page(pfn);
//                 pg->is_free = true;
//                 list_add_tail(&pg->page_list, &g_pmm.orders[target_order].free_list);
//                 g_pmm.orders[target_order].free_count++;
//                 g_pmm.free_pages += 1ULL << target_order;

//                 curr += (1ULL << (PAGE_SHIFT + target_order));
//             }
//         }
//     }

//     return 0;
// }

// core_initcall(pmm_init, 0);