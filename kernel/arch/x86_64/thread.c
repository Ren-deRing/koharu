#include <koharu/list.h>
#include <koharu/mmu.h>
#include <koharu/pmap.h>
#include <koharu/cpu.h>
#include <koharu/thread.h>
#include <koharu/kmem.h>

#include <asm/trapframe.h>
#include <asm/thread.h>
#include <asm/cpu.h>

#include <stdint.h>
#include <string.h>

uint32_t g_next_tid = 0;

struct thread *thread_create(pmap_t *pmap, uintptr_t entry, void *arg, uint32_t affinity) {
    struct thread *t = (struct thread*)kmalloc_aligned(sizeof(struct thread), 16);
    if (!t) return NULL;

    memset(t, 0, sizeof(*t));

    list_init((struct list_head *)&t->sched_list);
    list_init((struct list_head *)&t->ipc_list);

    t->pmap = pmap;

    void* stack = pmm_alloc_pages(3); // 32KB
    if (!stack) { kfree_aligned(t); return NULL; } 

    int status = pmap_map(t->pmap, USER_STACK_TOP - (32 * 1024), (uintptr_t)stack,
        32 * 1024, PROT_READ | PROT_WRITE | PROT_USER);

    if (status != 0) {
        pmm_free_pages(stack, 3);
        kfree_aligned(t);
        return NULL;
    }

    t->tf = (struct trapframe *)((uintptr_t)t->kernel_stack + THREAD_KSTACK_SIZE - sizeof(struct trapframe));

    t->tf->rip    = entry;
    t->tf->rdi    = (uintptr_t)arg;
    t->tf->rsp    = USER_STACK_TOP;
    t->tf->cs     = 0x23;
    t->tf->ss     = 0x1B;
    t->tf->rflags = 0x202;

    t->tid               = __atomic_fetch_add(&g_next_tid, 1, 5);
    t->pid               = 0;
    t->cpu_affinity      = affinity;
    t->total_cpu_time    = 0;
    t->time_quantum_left = 0;

    uint64_t *sp = (uint64_t *)t->tf;

    sp--; *sp = (uintptr_t)user_trampoline; // ret
    sp--; *sp = 0; // rbp
    sp--; *sp = 0; // rbx
    sp--; *sp = 0; // r12
    sp--; *sp = 0; // r13
    sp--; *sp = 0; // r14
    sp--; *sp = 0; // r15

    t->arch.rsp = (uintptr_t)sp;

    t->arch.xsaves_area = kmalloc_aligned(curcpu->arch_cpu_data->xsave_size, 64);
    memset(t->arch.xsaves_area, 0, curcpu->arch_cpu_data->xsave_size);

    uint64_t *xcomp_bv = (uint64_t *)((uint8_t *)t->arch.xsaves_area + 520); // 512 + 8
    *xcomp_bv = (1ULL << 63);

    sched_enqueue(t);

    return t;
}