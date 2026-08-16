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

struct syscall_ret { uint64_t extra[8]; };
uint64_t do_syscall(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, struct syscall_ret *ret);