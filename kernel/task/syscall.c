#include <koharu/print.h>

#include <stdint.h>

uint64_t do_syscall(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    dprintf("syscall: %lu, a1: %lu, a2: %lu, a3: %lu, a4: %lu\n", num, a1, a2, a3, a4);

    return 0;
}