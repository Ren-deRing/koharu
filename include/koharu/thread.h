#pragma once

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

    struct thread     *ipc_next;
    struct thread     *ipc_prev;

    struct thread     *next;
    struct thread     *prev;

    struct trapframe  *tf;
    struct arch_thread arch;
};