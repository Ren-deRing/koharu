#pragma once

#include <stdint.h>

#define SYS_EXIT        0
#define SYS_WRITE       1
#define SYS_SEND        2
#define SYS_RECV        3
#define SYS_CALL        4
#define SYS_REPLY_RECV  5
#define SYS_MAP         6
#define SYS_UNMAP       7
#define SYS_CREATE      8
#define SYS_START       9
#define SYS_GRANT      10
#define SYS_FUTEX_WAIT 11
#define SYS_FUTEX_WAKE 12

static inline long syscall0(uint64_t num) {
    register uint64_t rax asm("rax") = num;
    asm volatile("syscall"
        : "+a"(rax)
        :
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

static inline long syscall2(uint64_t num, uint64_t a1, uint64_t a2) {
    register uint64_t rax asm("rax") = num;
    asm volatile("syscall"
        : "+a"(rax)
        : "D"(a1), "S"(a2)
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

static inline long reply_recv(const uint64_t *reply, uint64_t *out) {
    register uint64_t rax asm("rax") = SYS_REPLY_RECV;
    register uint64_t rdi asm("rdi") = reply[0];
    register uint64_t rsi asm("rsi") = reply[1];
    register uint64_t rdx asm("rdx") = reply[2];
    register uint64_t r10 asm("r10") = reply[3];
    register uint64_t r8  asm("r8")  = reply[4];
    asm volatile("syscall"
        : "+a"(rax), "+D"(rdi), "+S"(rsi), "+d"(rdx), "+r"(r10), "+r"(r8)
        :
        : "rcx", "r11", "memory");
    out[0] = rdi;
    out[1] = rsi;
    out[2] = rdx;
    out[3] = r10;
    out[4] = r8;
    return (long)rax;
}
