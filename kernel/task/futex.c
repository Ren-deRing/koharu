#include <koharu/cpu.h>
#include <koharu/futex.h>
#include <koharu/initcall.h>
#include <koharu/kmem.h>
#include <koharu/mmu.h>
#include <koharu/pmap.h>
#include <koharu/sched.h>
#include <koharu/thread.h>

#include <stdint.h>

#define FUTEX_BUCKETS 64

#define EAGAIN  11
#define EFAULT  14
#define EINVAL  22

struct futex_waitq {
    list_node    hash_node;
    uintptr_t    phys;    // phys addr of the futex word
    list_head    waiters; // waiting threads (thread->futex_list)
};

static list_head futex_buckets[FUTEX_BUCKETS];

static size_t futex_hash(uintptr_t phys) {
    return (phys >> 2) & (FUTEX_BUCKETS - 1);
}

static struct futex_waitq *futex_find(uintptr_t phys) {
    list_head *bucket = &futex_buckets[futex_hash(phys)];

    for (list_node *n = bucket->next; n != (list_node *)bucket; n = n->next) {
        struct futex_waitq *wq = container_of(n, struct futex_waitq, hash_node);
        if (wq->phys == phys) return wq;
    }

    return NULL;
}

int futex_wait(void *uaddr, uint32_t expected) {
    struct thread *cur = curcpu->current;

    if ((uintptr_t)uaddr & 0x3) return -EINVAL;

    cpu_status_t flags = arch_irq_save();

    uintptr_t phys = pmap_extract(cur->pmap, (uintptr_t)uaddr);
    if (!phys) {
        arch_irq_restore(flags);
        return -EFAULT;
    }

    // value already changed? we are not going to sleep
    if (*(volatile uint32_t *)phys_to_virt(phys) != expected) {
        arch_irq_restore(flags);
        return -EAGAIN;
    }

    struct futex_waitq *wq = futex_find(phys);
    if (!wq) {
        wq = (struct futex_waitq *)kmalloc(sizeof(struct futex_waitq));
        if (!wq) {
            arch_irq_restore(flags);
            return -EAGAIN;
        }

        wq->phys = phys;
        list_init(&wq->waiters);
        list_add_tail(&wq->hash_node, &futex_buckets[futex_hash(phys)]);
    }

    list_add_tail(&cur->futex_list, &wq->waiters);

    sched_block();
    arch_irq_restore(flags);

    return 0;
}

int futex_wake(void *uaddr, int all) {
    struct thread *cur = curcpu->current;

    if ((uintptr_t)uaddr & 0x3) return -EINVAL;

    cpu_status_t flags = arch_irq_save();

    uintptr_t phys = pmap_extract(cur->pmap, (uintptr_t)uaddr);
    if (!phys) {
        arch_irq_restore(flags);
        return -EFAULT;
    }

    struct futex_waitq *wq = futex_find(phys);
    if (!wq) {
        arch_irq_restore(flags);
        return 0;
    }

    int woken = 0;
    while (!list_empty(&wq->waiters)) {
        list_node *n = wq->waiters.next;
        struct thread *t = container_of(n, struct thread, futex_list);

        list_del(n);
        sched_wakeup(t);
        woken++;

        if (!all) break;
    }

    if (list_empty(&wq->waiters)) { // last waiter gone.. release the waitq
        list_del(&wq->hash_node);
        kfree(wq);
    }

    arch_irq_restore(flags);

    return woken;
}

static int futex_init(void) {
    for (size_t i = 0; i < FUTEX_BUCKETS; i++)
        list_init(&futex_buckets[i]);

    return 0;
}

sys_initcall(futex_init, 0);
