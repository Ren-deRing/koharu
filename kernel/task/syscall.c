#include <koharu/print.h>
#include <koharu/thread.h>
#include <koharu/syscall.h>

#include <stdint.h>

uint64_t do_syscall(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, struct syscall_ret *ret) {
    switch (num) {
        case SYS_EXIT: {
            dprintf("report: min=%lu median=%lu avg=%lu\n", a1, a2, a3);
            break;
        }
        case SYS_SEND: {
            struct thread *target = thread_lookup(a1);
            if (!target) return (uint64_t)-1;
            uint64_t words[IPC_MSG_WORDS] = { a2, a3, a4, a5, a6 };
            return ipc_send(target, words);
        }
        case SYS_RECV: {
            uint64_t words[IPC_MSG_WORDS];
            uint64_t sender = ipc_recv(words);
            for (int i = 0; i < IPC_MSG_WORDS; i++) ret->extra[i] = words[i];
            return sender;
        }
        case SYS_CALL: {
            struct thread *target = thread_lookup(a1);
            if (!target) return (uint64_t)-1;
            uint64_t in[IPC_MSG_WORDS] = { a2, a3, a4, a5, a6 };
            uint64_t out[IPC_MSG_WORDS];
            uint64_t sender = ipc_call(target, in, out);
            for (int i = 0; i < IPC_MSG_WORDS; i++) ret->extra[i] = out[i];
            return sender;
        }
        case SYS_REPLY_RECV: {
            uint64_t reply[IPC_MSG_WORDS] = { a1, a2, a3, a4, a5 };
            uint64_t out[IPC_MSG_WORDS];
            uint64_t sender = ipc_reply_recv(reply, out);
            for (int i = 0; i < IPC_MSG_WORDS; i++) ret->extra[i] = out[i];
            return sender;
        }
        default:
            break;
    }

    return 0;
}