#pragma once

typedef enum ipc_endpoint_state {
    IPC_EP_INACTIVE,
    IPC_EP_WAIT_SEND,  // Clients waiting
    IPC_EP_WAIT_RECV   // Server waiting
} ipc_endpoint_state_t;

struct ipc_endpoint {
    ipc_endpoint_state_t state;

    struct thread *head;
    struct thread *tail;
};

typedef struct ipc_endpoint ipc_endpoint_t;