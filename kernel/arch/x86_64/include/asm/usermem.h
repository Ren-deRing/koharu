#pragma once

#include <stdint.h>
#include <stddef.h>

static inline int copy_from_user(void *dst, const void *src, size_t n) {
    int ok;
    asm volatile(
        "stac\n"
        "rep movsb\n"
        "clac\n"
        : "=&S"(src), "=&D"(dst), "=&c"(n), "=@ccc"(ok)
        : "0"(src), "1"(dst), "2"(n)
        : "memory"
    );
    return ok ? 0 : -1;
}

static inline int copy_to_user(void *dst, const void *src, size_t n) {
    int ok;
    asm volatile(
        "stac\n"
        "rep movsb\n"
        "clac\n"
        : "=&S"(src), "=&D"(dst), "=&c"(n), "=@ccc"(ok)
        : "0"(src), "1"(dst), "2"(n)
        : "memory"
    );
    return ok ? 0 : -1;
}

static inline int put_user_u64(uint64_t *dst, uint64_t val) {
    asm volatile("stac\n" "movq %1, (%0)\n" "clac\n"
        :
        : "r"(dst), "r"(val)
        : "memory"
    );
    return 0;
}

static inline int get_user_u64(uint64_t *src, uint64_t *dst) {
    asm volatile("stac\n" "movq (%1), %0\n" "clac\n"
        : "=&r"(*dst)
        : "r"(src)
        : "memory"
    );
    return 0;
}