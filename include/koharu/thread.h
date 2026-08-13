#pragma once

#include <koharu/list.h>
#include <koharu/pmap.h>

#include <asm/thread.h>
#include <asm/trapframe.h>

#include <stdint.h>

#define USER_STACK_TOP 0x7ffffffff000ULL
#define USER_CODE_BASE 0x100000ULL

typedef enum thread_state {
    THREAD_READY,       // ready (in RunQueue)
    THREAD_RUNNING,     // running
    THREAD_BLOCKED,     // IPC waiting
    THREAD_TERMINATED   // ready to term
} thread_state_t;

struct thread {
    uint64_t           tid;
    uint64_t           pid;
    thread_state_t     state;

    uint32_t           current_level;
    uint32_t           time_quantum_left;
    uint64_t           total_cpu_time;
    
    uint32_t           cpu_affinity;

    list_node          sched_list;
    list_node          ipc_list;

    pmap_t            *pmap;
    struct trapframe  *tf;
    struct arch_thread arch;
};

struct thread *thread_create(pmap_t *pmap, uintptr_t entry, void *arg, uint32_t affinity);
void thread_exit(void);
void sched_yield(void);
void sched_tick(void);
void sched_init(void);
void sched_boot(void);