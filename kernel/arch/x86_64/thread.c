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
    struct thread *t = (struct thread*)kmalloc(sizeof(struct thread));
    struct trapframe *tf = (struct trapframe*)kmalloc(sizeof(struct trapframe));
    if (!t || !tf) return NULL;

    // TODO: fail free

    memset(t, 0, sizeof(*t));
    memset(tf, 0, sizeof(*tf));

    list_init((struct list_head *)&t->sched_list);
    list_init((struct list_head *)&t->ipc_list);

    t->pmap = pmap;

    void* stack = pmm_alloc_pages(3); // 32KB

    pmap_map(t->pmap, USER_STACK_TOP - (32 * 1024), (uintptr_t)stack, 32 * 1024, PROT_READ | PROT_WRITE | PROT_USER);

    tf->rip    = entry;
    tf->rdi    = (uintptr_t)arg;
    tf->rsp    = USER_STACK_TOP;
    tf->cs     = 0x23;
    tf->ss     = 0x1B;
    tf->rflags = 0x202;
    
    t->tf = tf;

    t->tid               = __atomic_fetch_add(&g_next_tid, 1, 5);
    t->pid               = 0;
    t->cpu_affinity      = affinity;
    t->total_cpu_time    = 0;
    t->state             = THREAD_READY;
    t->time_quantum_left = 0;

    t->arch.xsaves_area = kmalloc_aligned(curcpu->arch_cpu_data->xsave_size, 64);

    // TODO: sched_enqueue

    return t;
}