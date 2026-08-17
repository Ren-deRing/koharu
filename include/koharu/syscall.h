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

struct syscall_ret { uint64_t extra[8]; };
uint64_t do_syscall(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, struct syscall_ret *ret);