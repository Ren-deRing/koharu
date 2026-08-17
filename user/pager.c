#include <pager.h>
#include <elf.h>
#include <root.h>
#include <syscall.h>
#include <vma.h>
#include <console.h>

#include <stddef.h>
#include <string.h>
#include <stdint.h>

static uint64_t self_tid;
static uint8_t *pool;
static uint64_t pool_bits;
static uint64_t pool_bit;
static struct vma_set child_vmas;

void pager_init(uint64_t tid, uint64_t frame_count) {
    self_tid  = tid;
    pool      = (uint8_t *)ROOT_POOL_VA;
    pool_bits = frame_count;
    pool_bit  = 0;

    vma_set_init(&child_vmas);
}

static int pool_alloc(uint64_t *pfn_out) {
    uint64_t scanned = 0;

    while (scanned < pool_bits) {
        if (pool_bit >= pool_bits) pool_bit = 0;
        if (pool[pool_bit / 8] & (1u << (pool_bit % 8))) {
            *pfn_out = pool_bit;
            pool_bit++;
            return 0;
        }
        pool_bit++;
        scanned++;
    }

    return -1;
}

static int map_page(uint64_t target, uint64_t va, uint64_t phys, uint64_t prot) {
    return syscall5(SYS_MAP, target, va, phys, PAGE_SIZE, prot) == 0 ? 0 : -1;
}

static int frame_zero(uint64_t phys) {
    if (map_page(self_tid, SCRATCH_VA, phys, PROT_READ | PROT_WRITE) != 0) return -1;
    memset((void *)SCRATCH_VA, 0, PAGE_SIZE);
    syscall3(SYS_UNMAP, self_tid, SCRATCH_VA, PAGE_SIZE);
    return 0;
}

int load_segment(uint64_t child, const uint8_t *elf, const struct elf64_phdr *ph) {
    uint64_t prot = 0;
    if (ph->p_flags & PF_R) prot |= PROT_READ;
    if (ph->p_flags & PF_W) prot |= PROT_WRITE;
    if (ph->p_flags & PF_X) prot |= PROT_EXEC;

    uint64_t va_start = ph->p_vaddr & ~(PAGE_SIZE - 1);
    uint64_t va       = va_start;
    uint64_t va_end   = (ph->p_vaddr + ph->p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (; va < va_end; va += PAGE_SIZE) {
        uint64_t pfn;
        if (pool_alloc(&pfn) != 0) return -1;
        uint64_t phys = pfn << 12;

        if (frame_zero(phys) != 0) return -1;
        if (map_page(self_tid, SCRATCH_VA, phys, PROT_READ | PROT_WRITE) != 0) return -1;

        uint64_t f_lo = va > ph->p_vaddr ? va : ph->p_vaddr;
        uint64_t f_hi = (va + PAGE_SIZE) < (ph->p_vaddr + ph->p_filesz)
                       ? (va + PAGE_SIZE) : (ph->p_vaddr + ph->p_filesz);
        if (f_hi > f_lo) {
            memcpy((void *)(SCRATCH_VA + (f_lo - va)),
                   elf + ph->p_offset + (f_lo - ph->p_vaddr), f_hi - f_lo);
        }

        syscall3(SYS_UNMAP, self_tid, SCRATCH_VA, PAGE_SIZE);

        if (map_page(child, va, phys, prot) != 0) return -1;
    }

    return vma_insert(&child_vmas, va_start, va_end, prot,
                      (ph->p_flags & PF_X) ? VMA_CODE : VMA_DATA);
}

int map_shared(uint64_t child) {
    uint64_t pfn;
    if (pool_alloc(&pfn) != 0) return -1;
    uint64_t phys = pfn << 12;

    if (frame_zero(phys) != 0) return -1;
    if (map_page(self_tid, SHARED_VA, phys, PROT_READ | PROT_WRITE) != 0) return -1;
    if (map_page(child, SHARED_VA, phys, PROT_READ | PROT_WRITE) != 0) return -1;

    return 0;
}

int map_utcb(uint64_t child, uint64_t child_tid, uint64_t pager_tid) {
    uint64_t pfn;
    if (pool_alloc(&pfn) != 0) return -1;
    uint64_t phys = pfn << 12;

    if (frame_zero(phys) != 0) return -1;
    if (map_page(child, UTBC_VA, phys, PROT_READ | PROT_WRITE) != 0) return -1;

    uint64_t *utcb = (uint64_t *)SCRATCH_VA;
    if (map_page(self_tid, SCRATCH_VA, phys, PROT_READ | PROT_WRITE) != 0) return -1;
    utcb[0] = child_tid;
    utcb[1] = pager_tid;
    syscall3(SYS_UNMAP, self_tid, SCRATCH_VA, PAGE_SIZE);

    return 0;
}

int map_stack(uint64_t child) {
    uint64_t bottom = CHILD_STACK_TOP - CHILD_STACK_SIZE;

    for (uint64_t va = bottom; va < CHILD_STACK_TOP; va += PAGE_SIZE) {
        uint64_t pfn;
        if (pool_alloc(&pfn) != 0) return -1;
        uint64_t phys = pfn << 12;

        if (frame_zero(phys) != 0) return -1;
        if (map_page(child, va, phys, PROT_READ | PROT_WRITE) != 0) return -1;
    }

    return vma_insert(&child_vmas, bottom, CHILD_STACK_TOP, PROT_READ | PROT_WRITE, VMA_STACK);
}

void serve(uint64_t child) {
    static uint64_t heap = CHILD_HEAP_VA;
    uint64_t reply[5] = { 0, 0, 0, 0, 0 };

    vma_dump(&child_vmas);

    for (;;) {
        uint64_t msg[5];
        long sender = reply_recv(reply, msg);

        reply[0] = reply[1] = reply[2] = reply[3] = reply[4] = 0;

        if (sender <= 0 || child == 0) continue;

        if (msg[0] == KOHARU_REQ_READY) {
            log_str("[root] child ready\n");
            continue;
        }

        if (msg[0] == KOHARU_REQ_EXIT) {
            log_str("[root] child exited\n");
            return;
        }

        if (msg[0] == KOHARU_REQ_ALLOC || msg[0] == KOHARU_REQ_MAP) {
            uint64_t pages = (msg[1] + PAGE_SIZE - 1) >> 12;
            int ok = 1;

            for (uint64_t i = 0; i < pages; i++) {
                uint64_t pfn;
                if (pool_alloc(&pfn) != 0) { ok = 0; break; }
                uint64_t phys = pfn << 12;

                if (frame_zero(phys) != 0) { ok = 0; break; }
                if (map_page(child, heap + i * PAGE_SIZE, phys, PROT_READ | PROT_WRITE) != 0) {
                    ok = 0;
                    break;
                }
            }

            if (ok) {
                reply[0] = heap;
                if (vma_insert(&child_vmas, heap, heap + pages * PAGE_SIZE,
                               PROT_READ | PROT_WRITE, VMA_HEAP) != 0) {
                    ok = 0;
                }
            }

            if (ok) {
                heap += pages * PAGE_SIZE;
            }
        }
    }
}