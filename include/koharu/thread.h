#pragma once

#include <koharu/list.h>

#include <asm/thread.h>
#include <asm/trapframe.h>

#include <stdint.h>

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

    struct trapframe  *tf;
    struct arch_thread arch;
};