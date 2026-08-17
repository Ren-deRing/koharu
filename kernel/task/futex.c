#include <koharu/cpu.h>
#include <koharu/futex.h>
#include <koharu/initcall.h>
#include <koharu/kmem.h>
#include <koharu/lock.h>
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
static spinlock_t futex_bucket_locks[FUTEX_BUCKETS];

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

    uintptr_t phys = pmap_extract(cur->pmap, (uintptr_t)uaddr);
    if (!phys) return -EFAULT;

    // value already changed? we are not going to sleep
    if (*(volatile uint32_t *)phys_to_virt(phys) != expected)
        return -EAGAIN;

    size_t bucket = futex_hash(phys);
    uint64_t flags = spin_lock_irqsave(&futex_bucket_locks[bucket]);

    struct futex_waitq *wq = futex_find(phys);
    if (!wq) {
        wq = (struct futex_waitq *)kmalloc(sizeof(struct futex_waitq));
        if (!wq) {
            spin_unlock_irqrestore(&futex_bucket_locks[bucket], flags);
            return -EAGAIN;
        }

        wq->phys = phys;
        list_init(&wq->waiters);
        list_add_tail(&wq->hash_node, &futex_buckets[bucket]);
    }

    list_add_tail(&cur->futex_list, &wq->waiters);

    spin_unlock_irqrestore(&futex_bucket_locks[bucket], flags);

    sched_block();

    return 0;
}

int futex_wake(void *uaddr, int all) {
    struct thread *cur = curcpu->current;

    if ((uintptr_t)uaddr & 0x3) return -EINVAL;

    uintptr_t phys = pmap_extract(cur->pmap, (uintptr_t)uaddr);
    if (!phys) return -EFAULT;

    size_t bucket = futex_hash(phys);
    uint64_t flags = spin_lock_irqsave(&futex_bucket_locks[bucket]);

    struct futex_waitq *wq = futex_find(phys);
    if (!wq) {
        spin_unlock_irqrestore(&futex_bucket_locks[bucket], flags);
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

    if (list_empty(&wq->waiters)) {
        list_del(&wq->hash_node);
        kfree(wq);
    }

    spin_unlock_irqrestore(&futex_bucket_locks[bucket], flags);

    return woken;
}

static int futex_init(void) {
    for (size_t i = 0; i < FUTEX_BUCKETS; i++) {
        list_init(&futex_buckets[i]);
        spin_lock_init(&futex_bucket_locks[i]);
    }

    return 0;
}

sys_initcall(futex_init, 0);
