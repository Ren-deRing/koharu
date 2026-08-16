#include <console.h>
#include <syscall.h>

#include <string.h>
#include <stdint.h>

void log_str(const char *s) {
    syscall3(SYS_WRITE, 1, (uint64_t)s, (uint64_t)strlen(s));
}

void log_hex(uint64_t v) {
    char buf[16];
    for (int i = 15; i >= 0; i--) {
        int d = (int)(v & 0xF);
        buf[i] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        v >>= 4;
    }
    log_str("0x");
    syscall3(SYS_WRITE, 1, (uint64_t)buf, 16);
}

void log_ulong(uint64_t v) {
    char buf[21];
    int i = 20;
    buf[i--] = '\0';
    do {
        buf[i--] = '0' + (v % 10);
        v /= 10;
    } while (v > 0);
    log_str(&buf[i + 1]);
}