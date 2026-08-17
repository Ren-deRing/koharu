#pragma once

#include <stdint.h>

#define SYS_THREAD_CONTROL      0
#define SYS_EXCHANGE_REGISTERS  1
#define SYS_IPC                 2
#define SYS_MAP                 3
#define SYS_GRANT               4
#define SYS_UNMAP               5
#define SYS_SCHEDULE            6
#define SYS_THREAD_SWITCH       7
#define SYS_WRITE               8
#define SYS_FUTEX               9
#define SYS_PROCESSOR_CONTROL  10
#define SYS_EXCEPTION_HANDLER  11
#define SYS_SYSTEM_CONTROL     12
#define SYS_SPACE_CONTROL      13

// ThreadControl flags
#define TC_CREATE     0
#define TC_DESTROY    1
#define TC_BIND_SPACE 2

// ExchangeRegisters flags
#define EXR_READ       0
#define EXR_WRITE      1
#define EXR_ACTIVATE   2
#define EXR_DEACTIVATE 3
#define EXR_SET_ENTRY  4

// IPC flags
#define IPC_SEND           0
#define IPC_RECV           1
#define IPC_CALL           2
#define IPC_REPLY_AND_WAIT 3

// Futex ops
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

// ExceptionHandler ops
#define EXC_SET_PAGER 0
#define EXC_UNSET     1

// SystemControl ops
#define SYS_REBOOT   0
#define SYS_SHUTDOWN 1

// ProcessorControl ops
#define PROC_QUERY  0
#define PROC_START  1
#define PROC_STOP   2

// encoding: rdi = (op << 32) | target_tid
#define IPC_PACK(op, target) (((uint64_t)(op) << 32) | (uint64_t)(target))

static inline long syscall0(uint64_t num) {
    register uint64_t rax asm("rax") = num;
    asm volatile("syscall"
        : "+a"(rax)
        :
        : "rcx", "r11", "memory");
    return (long)rax;
}

static inline long syscall2(uint64_t num, uint64_t a1, uint64_t a2) {
    register uint64_t rax asm("rax") = num;
    asm volatile("syscall"
        : "+a"(rax)
        : "D"(a1), "S"(a2)
        : "rcx", "r11", "memory");
    return (long)rax;
}

static inline long syscall3(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3) {
    register uint64_t rax asm("rax") = num;
    asm volatile("syscall"
        : "+a"(rax)
        : "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory");
    return (long)rax;
}

static inline long syscall4(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    register uint64_t rax asm("rax") = num;
    register uint64_t r10 asm("r10") = a4;
    asm volatile("syscall"
        : "+a"(rax)
        : "D"(a1), "S"(a2), "d"(a3), "r"(r10)
        : "rcx", "r11", "memory");
    return (long)rax;
}

static inline long syscall5(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    register uint64_t rax asm("rax") = num;
    register uint64_t r10 asm("r10") = a4;
    register uint64_t r8  asm("r8")  = a5;
    asm volatile("syscall"
        : "+a"(rax)
        : "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory");
    return (long)rax;
}

static inline long koharu_ipc(uint64_t op, uint64_t target, const uint64_t *in, uint64_t *out) {
    register uint64_t rax asm("rax") = SYS_IPC;
    register uint64_t rdi asm("rdi") = IPC_PACK(op, target);
    register uint64_t rsi asm("rsi") = in[0];
    register uint64_t rdx asm("rdx") = in[1];
    register uint64_t r10 asm("r10") = in[2];
    register uint64_t r8  asm("r8")  = in[3];
    register uint64_t r9  asm("r9")  = in[4];
    asm volatile("syscall"
        : "+a"(rax), "+D"(rdi), "+S"(rsi), "+d"(rdx), "+r"(r10), "+r"(r8), "+r"(r9)
        :
        : "rcx", "r11", "memory");
    out[0] = rdi;
    out[1] = rsi;
    out[2] = rdx;
    out[3] = r10;
    out[4] = r8;
    return (long)rax;
}

static inline long reply_recv(const uint64_t *reply, uint64_t *out) {
    return koharu_ipc(IPC_REPLY_AND_WAIT, 0, reply, out);
}
