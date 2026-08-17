#include <koharu/intc.h>
#include <koharu/initcall.h>
#include <koharu/print.h>
#include <koharu/sched.h>
#include <koharu/cpu.h>
#include <koharu/thread.h>

#include <stdatomic.h>
#include <stdbool.h>

static _Atomic uint64_t sched_ticks = 0;

static void sched_boost(void) {
    struct thread *cur = curcpu->current;
    if (cur) {
        cur->current_level     = 0;
        cur->time_quantum_left = QUANTUM_TICKS[0];
    }

    for (int level = 1; level < MLFQ_LEVELS; level++) {
        list_head *q = &curcpu->runq[level];
        while (!list_empty(q)) {
            list_node *n = q->next;
            struct thread *t = container_of(n, struct thread, sched_list);
            list_del(n);
            t->current_level     = 0;
            t->time_quantum_left = QUANTUM_TICKS[0];
            list_add_tail(n, &curcpu->runq[0]);
        }
    }
}

static struct thread *sched_pick(void) {
    for (int level = 0; level < MLFQ_LEVELS; level++) {
        if (list_empty(&curcpu->runq[level])) continue;
        list_node *n = curcpu->runq[level].next;
        list_del(n);
        return container_of(n, struct thread, sched_list);
    }
    return NULL;
}

static void sched_int(struct trapframe *regs, void *data) {
    (void)regs, (void)data;

    sched_tick();
}

void sched_tick(void) {
    g_intc->eoi();

    struct thread *cur = curcpu->current;
    bool cur_exhausted = false;

    if (cur && cur->state == THREAD_RUNNING) {
        cur->total_cpu_time++;
        if (--cur->time_quantum_left == 0) {
            cur_exhausted = true;
            if (cur->current_level < MLFQ_LEVELS - 1) cur->current_level++;
            cur->time_quantum_left = QUANTUM_TICKS[cur->current_level];
            cur->state = THREAD_READY;
            list_add_tail(&cur->sched_list, &curcpu->runq[cur->current_level]);
        }
    }

    if (atomic_fetch_add(&sched_ticks, 1) % BOOST_PERIOD == (BOOST_PERIOD - 1))
        sched_boost();

    if (!cur_exhausted) return;

    struct thread *next = sched_pick();
    if (!next) return;
    if (next == cur) { cur->state = THREAD_RUNNING; return; }

    // dprintf("t%lu -> t%lu\n", cur->tid, next->tid);
    // dprintf("cur kernel_stack: %p\n", &cur->kernel_stack);
    // dprintf("next kernel_stack: %p\n", &next->kernel_stack);

    curcpu->current = next;
    next->state = THREAD_RUNNING;
    switch_to(cur, next);
}

void sched_yield(void) {
    cpu_status_t flags = arch_irq_save();
    struct thread *cur = curcpu->current;

    cur->time_quantum_left = QUANTUM_TICKS[cur->current_level];
    cur->state = THREAD_READY;
    list_add_tail(&cur->sched_list, &curcpu->runq[cur->current_level]);

    struct thread *next = sched_pick();
    if (!next || next == cur) { cur->state = THREAD_RUNNING; arch_irq_restore(flags); return; }

    curcpu->current = next;
    next->state = THREAD_RUNNING;
    switch_to(cur, next);

    arch_irq_restore(flags);
}

void sched_block(void) {
    struct thread *cur = curcpu->current;

    cur->state = THREAD_BLOCKED;

    struct thread *next = sched_pick();
    if (!next) {
        dprintf("sched: no runnable thread\n");
        for (;;) arch_halt();
    }

    curcpu->current = next;
    next->state = THREAD_RUNNING;
    switch_to(cur, next);
}

void sched_exit(void) {
    struct thread *cur = curcpu->current;

    cur->state = THREAD_TERMINATED;

    struct thread *next = sched_pick();
    if (!next) {
        dprintf("sched: no runnable thread\n");
        for (;;) arch_halt();
    }

    curcpu->current = next;
    next->state = THREAD_RUNNING;
    switch_to(cur, next);
}

void sched_enqueue(struct thread *t) {
    t->current_level     = 0;
    t->state             = THREAD_READY;
    t->time_quantum_left = QUANTUM_TICKS[0];
    list_add_tail(&t->sched_list, &curcpu->runq[0]);
}

void sched_dequeue(struct thread *t) {
    list_del(&t->sched_list);
}

void sched_wakeup(struct thread *t) {
    sched_enqueue(t); // anyway, it's a same thing
}

void sched_boot(void) {
    struct thread *first = sched_pick();
    if (!first) { dprintf("sched: no runnable thread\n"); return; }

    curcpu->current = first;
    first->state = THREAD_RUNNING;
    arch_irq_enable();
    g_intc->timer_periodic(TIMER_VECTOR, TICK_HZ);
    switch_to_first(first);
}

int sched_init(void) {
    for (int i = 0; i < MLFQ_LEVELS; i++)
        list_init(&curcpu->runq[i]);

    register_handler(TIMER_VECTOR, sched_int, NULL);

    return 0;
}

late_initcall(sched_init, 0);