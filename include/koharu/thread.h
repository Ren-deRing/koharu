#pragma once

#include <koharu/list.h>
#include <koharu/pmap.h>
#include <koharu/ipc.h>

#include <asm/thread.h>
#include <asm/trapframe.h>

#include <stdint.h>

#define THREAD_KSTACK_SIZE (1024 * 16)

#define USER_STACK_TOP 0x7ffffffff000ULL
#define USER_CODE_BASE 0x100000ULL

typedef enum thread_state {
    THREAD_READY,       // ready (in RunQueue)
    THREAD_RUNNING,     // running
    THREAD_BLOCKED,     // IPC waiting
    THREAD_TERMINATED   // ready to term
} thread_state_t;

struct thread {
    struct arch_thread  arch;
    pmap_t             *pmap;
    struct trapframe   *tf;

    uint64_t            tid;
    uint64_t            pid;
    thread_state_t      state;

    uint32_t            current_level;
    uint32_t            time_quantum_left;
    uint64_t            total_cpu_time;
    
    uint32_t            cpu_affinity;

    list_node           sched_list;
    list_node           ipc_list;

    uint8_t             kernel_stack[THREAD_KSTACK_SIZE];

    struct ipc_endpoint ipc_ep;
    uint32_t            home_cpu;

    uint64_t            ipc_msg[IPC_MSG_WORDS]; // pending send / received msg
    uint64_t            ipc_sender;             // sender tid of received msg

    uint64_t            pager_tid;
};

void switch_to(struct thread *prev, struct thread *next);
void switch_to_first(struct thread *next);
void user_trampoline();

struct thread *thread_create(pmap_t *pmap, uintptr_t entry, void *arg, uint32_t affinity);
struct thread *thread_create_child(pmap_t *pmap, uintptr_t entry);
void thread_exit(void);

struct thread *thread_lookup(uint64_t tid);
uint32_t thread_alloc_tid(void);
int thread_register(struct thread *t);