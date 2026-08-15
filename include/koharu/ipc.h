#pragma once

#include <koharu/list.h>
#include <stdint.h>

#define IPC_MSG_WORDS 5

struct thread;

typedef enum ipc_endpoint_state {
    IPC_EP_INACTIVE,
    IPC_EP_WAIT_SEND,  // Clients waiting
    IPC_EP_WAIT_RECV   // Server waiting
} ipc_endpoint_state_t;

struct ipc_endpoint {
    ipc_endpoint_state_t state;

    list_node            ipc_waitqueue;
};

typedef struct ipc_endpoint ipc_endpoint_t;

int ipc_send(struct thread *target, const uint64_t words[IPC_MSG_WORDS]);
uint64_t ipc_recv(uint64_t words[IPC_MSG_WORDS]);
uint64_t ipc_call(struct thread *target, const uint64_t in[IPC_MSG_WORDS], uint64_t out[IPC_MSG_WORDS]);
uint64_t ipc_reply_recv(const uint64_t reply[IPC_MSG_WORDS], uint64_t out[IPC_MSG_WORDS]);