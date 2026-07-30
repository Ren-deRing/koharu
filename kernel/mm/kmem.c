#include <koharu/assert.h>
#include <koharu/cpu.h>
#include <koharu/initcall.h>
#include <koharu/tlsf.h>
#include <koharu/mmu.h>

#include <string.h>

void* kmalloc(size_t size) {
    // fast path
    void *ptr = tlsf_malloc(curcpu->tlsf_ctrl, size);
    if (ptr) return ptr;

    // slow path

    // clear pending list
    tlsf_flush_pending(curcpu);
    ptr = tlsf_malloc(curcpu->tlsf_ctrl, size);
    if (ptr) return ptr;

    // buddy! help me!
    size_t alloc_bytes = size + 64; // margin
    size_t pages = (alloc_bytes + PAGE_SIZE - 1) >> PAGE_SHIFT;
    size_t req_order = (pages <= 1) ? 0 : 32 - __builtin_clz((uint32_t)(pages - 1));

    size_t order = (req_order < 4) ? 4 : req_order; // maintain min order (4)

    void *phys_ptr = pmm_alloc_pages(order); // BUDDY!
    if (!phys_ptr && (uintptr_t)phys_ptr == 0) return NULL; // OOM

    size_t page_count = 1ULL << order;
    size_t base_pfn = phys_to_pfn((uintptr_t)phys_ptr);

    // write this heap's owner
    for (size_t i = 0; i < page_count; i++) {
        page_t *pg = pfn_to_page(base_pfn + i);
        pg->owner_cpu_id = curcpu->id;
    }

    void *pool_ptr = phys_to_virt((uintptr_t)phys_ptr);
    size_t pool_bytes = page_count << PAGE_SHIFT;

    tlsf_add_pool(curcpu->tlsf_ctrl, pool_ptr, pool_bytes);

    return tlsf_malloc(curcpu->tlsf_ctrl, size);
}

void kfree(void *ptr) {
    if (!ptr) return;
    
    page_t *page = virt_to_page(ptr);
    struct cpu *owner = id_to_cpu(page->owner_cpu_id);

    if (curcpu == owner) { // if it's my heap
        tlsf_free(curcpu->tlsf_ctrl, ptr); // just free to tlsf pool
        return;
    }

    // not my heap? pass the work off to owner
    tlsf_push_pending(owner, ptr);
}

int percpu_tlsf_init(void) {
    int order = 4; // 64KB
    size_t page_count = 1ULL << order;
    size_t init_pool_size = page_count << PAGE_SHIFT;

    void *phys_ptr = pmm_alloc_pages(order);
    if (!phys_ptr && (uintptr_t)phys_ptr == 0) return -1;

    size_t base_pfn = phys_to_pfn((uintptr_t)phys_ptr);
    for (size_t i = 0; i < page_count; i++) {
        page_t *pg = pfn_to_page(base_pfn + i);
        pg->owner_cpu_id = curcpu->id;
    }

    void *pool_ptr = phys_to_virt((uintptr_t)phys_ptr);
    curcpu->tlsf_ctrl = tlsf_create_with_pool(pool_ptr, init_pool_size);

    return 0;
}

core_initcall(percpu_tlsf_init, 1); // hmm, i have no idea how APs should call this function...