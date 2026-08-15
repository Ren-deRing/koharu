#include <stdint.h>

#define SYS_EXIT 0
#define SYS_CALL 5
#define SYS_REPLY_RECV 6
#define SERVER_TID 0

#define N_WARMUP 100
#define N_ITER   1000

static inline uint64_t sys_exit(uint64_t a1, uint64_t a2, uint64_t a3) {
    register uint64_t rax asm("rax") = SYS_EXIT;
    asm volatile("syscall"
        : "+a"(rax)
        : "D"(a1), "S"(a2), "d"(a3) // a3 -> rdx
        : "rcx", "r11", "memory");
    return rax;
}

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    asm volatile("lfence; rdtsc; lfence" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t sys_call(uint64_t target, const uint64_t w[5], uint64_t r[5]) {
    register uint64_t wo0 asm("rdi") = target;
    register uint64_t wo1 asm("rsi") = w[0];
    register uint64_t wo2 asm("rdx") = w[1];
    register uint64_t wo3 asm("r10") = w[2];
    register uint64_t wo4 asm("r8")  = w[3];
    register uint64_t r9_ asm("r9")  = w[4];
    register uint64_t rax asm("rax") = SYS_CALL;
    asm volatile("syscall"
        : "+a"(rax), "+D"(wo0), "+S"(wo1), "+d"(wo2), "+r"(wo3), "+r"(wo4)
        : "r"(r9_)
        : "rcx", "r11", "memory");
    r[0]=wo0; r[1]=wo1; r[2]=wo2; r[3]=wo3; r[4]=wo4;
    return rax; // responder tid
}

static inline uint64_t sys_reply_recv(const uint64_t reply[5], uint64_t r[5]) {
    register uint64_t wo0 asm("rdi") = reply[0];
    register uint64_t wo1 asm("rsi") = reply[1];
    register uint64_t wo2 asm("rdx") = reply[2];
    register uint64_t wo3 asm("r10") = reply[3];
    register uint64_t wo4 asm("r8")  = reply[4];
    register uint64_t rax asm("rax") = SYS_REPLY_RECV;
    asm volatile("syscall"
        : "+a"(rax), "+D"(wo0), "+S"(wo1), "+d"(wo2), "+r"(wo3), "+r"(wo4)
        : : "rcx", "r11", "memory");
    r[0]=wo0; r[1]=wo1; r[2]=wo2; r[3]=wo3; r[4]=wo4;
    return rax; // sender tid
}

__attribute__((section("entry")))
void _start(uint64_t arg) {
    if (arg == 0) {
        uint64_t w[5], ack[5] = { 0, 0, 0, 0, 0 };
        for (;;) {
            sys_reply_recv(ack, w); // reply to caller
            ack[0] = w[0] ^ 0xA5A5;
        }
    } else {
        uint64_t deltas[N_ITER];
        uint64_t min = UINT64_MAX, sum = 0;

        uint64_t msg[5] = { 0xDEADBEEF, 1, 2, 3, 4 };
        uint64_t r[5];

        for (int i = 0; i < N_ITER; i++) {
            uint64_t t0 = rdtsc();
            sys_call(SERVER_TID, msg, r);
            uint64_t t1 = rdtsc();
            deltas[i] = t1 - t0;
            if (deltas[i] < min) min = deltas[i];
            sum += deltas[i];
        }

        for (int i = 1; i < N_ITER; i++) {
            uint64_t key = deltas[i];
            int j = i - 1;
            while (j >= 0 && deltas[j] > key) { deltas[j + 1] = deltas[j]; j--; }
            deltas[j + 1] = key;
        }
        uint64_t median = deltas[N_ITER / 2];
        uint64_t avg    = sum / N_ITER;

        sys_exit(min, median, avg); // cycles
    }
    
    for (;;);
}