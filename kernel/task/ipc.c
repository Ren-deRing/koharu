#include <koharu/cpu.h>
#include <koharu/sched.h>
#include <koharu/thread.h>
#include <koharu/ipc.h>
#include <string.h>

static void ipc_deliver_to_recv(struct thread *recv, struct thread *snd, const uint64_t *words) {
    list_del(recv->ipc_ep.ipc_waitqueue.next);

    memcpy(recv->ipc_msg, words, sizeof(uint64_t) * IPC_MSG_WORDS);
    recv->ipc_sender = snd->tid;

    if (list_empty((list_head *)&recv->ipc_ep.ipc_waitqueue))
        recv->ipc_ep.state = IPC_EP_INACTIVE;

    sched_wakeup(recv);
}

static void ipc_accept_sender(struct thread *recv, struct thread *snd) {
    list_del(&snd->ipc_list);

    memcpy(recv->ipc_msg, snd->ipc_msg, sizeof(uint64_t) * IPC_MSG_WORDS);
    recv->ipc_sender = snd->tid;

    if (snd->ipc_ep.state == IPC_EP_WAIT_RECV) {
        // waiting for a reply
        list_add_tail(&snd->ipc_list, (list_head *)&snd->ipc_ep.ipc_waitqueue);
    } else {
        sched_wakeup(snd);
    }
}

int ipc_send(struct thread *target, const uint64_t words[IPC_MSG_WORDS]) {
    struct thread *cur = curcpu->current;
    struct ipc_endpoint *ep = &target->ipc_ep;

    cpu_status_t flags = arch_irq_save();

    if (ep->state == IPC_EP_WAIT_RECV) {
        struct thread *recv = container_of(ep->ipc_waitqueue.next, struct thread, ipc_list);
        ipc_deliver_to_recv(recv, cur, words);
        arch_irq_restore(flags);
        return 0;
    }

    memcpy(cur->ipc_msg, words, sizeof(uint64_t) * IPC_MSG_WORDS);
    ep->state = IPC_EP_WAIT_SEND;
    list_add_tail(&cur->ipc_list, (list_head *)&ep->ipc_waitqueue);

    sched_block();
    arch_irq_restore(flags);
    return 0;
}

uint64_t ipc_recv(uint64_t words[IPC_MSG_WORDS]) {
    struct thread *cur = curcpu->current;
    struct ipc_endpoint *ep = &cur->ipc_ep;

    cpu_status_t flags = arch_irq_save();

    if (ep->state == IPC_EP_WAIT_SEND) {
        struct thread *snd = container_of(ep->ipc_waitqueue.next, struct thread, ipc_list);
        uint64_t sender_tid = snd->tid;

        ipc_accept_sender(cur, snd);

        memcpy(words, cur->ipc_msg, sizeof(uint64_t) * IPC_MSG_WORDS);
        arch_irq_restore(flags);
        return sender_tid;
    }

    ep->state = IPC_EP_WAIT_RECV;
    list_add_tail(&cur->ipc_list, (list_head *)&ep->ipc_waitqueue);

    sched_block();
    arch_irq_restore(flags);

    memcpy(words, cur->ipc_msg, sizeof(uint64_t) * IPC_MSG_WORDS);
    return cur->ipc_sender;
}

uint64_t ipc_call(struct thread *target, const uint64_t in[IPC_MSG_WORDS], uint64_t out[IPC_MSG_WORDS]) {
    struct thread *cur = curcpu->current;

    cpu_status_t flags = arch_irq_save();

    cur->ipc_ep.state = IPC_EP_WAIT_RECV;

    if (target->ipc_ep.state == IPC_EP_WAIT_RECV) {
        struct thread *server = container_of(target->ipc_ep.ipc_waitqueue.next, struct thread, ipc_list);
        ipc_deliver_to_recv(server, cur, in);
        list_add_tail(&cur->ipc_list, (list_head *)&cur->ipc_ep.ipc_waitqueue);
    } else {
        memcpy(cur->ipc_msg, in, sizeof(uint64_t) * IPC_MSG_WORDS);
        target->ipc_ep.state = IPC_EP_WAIT_SEND;
        list_add_tail(&cur->ipc_list, (list_head *)&target->ipc_ep.ipc_waitqueue);
    }

    sched_block();
    arch_irq_restore(flags);

    memcpy(out, cur->ipc_msg, sizeof(uint64_t) * IPC_MSG_WORDS);
    return cur->ipc_sender;
}

uint64_t ipc_reply_recv(const uint64_t reply[IPC_MSG_WORDS], uint64_t out[IPC_MSG_WORDS]) {
    struct thread *cur = curcpu->current;

    cpu_status_t flags = arch_irq_save();

    struct thread *caller = thread_lookup(cur->ipc_sender);
    if (caller && caller != cur && caller->ipc_ep.state == IPC_EP_WAIT_RECV) {
        ipc_deliver_to_recv(caller, cur, reply);
    }

    cur->ipc_ep.state = IPC_EP_WAIT_RECV;
    list_add_tail(&cur->ipc_list, (list_head *)&cur->ipc_ep.ipc_waitqueue);

    sched_block();
    arch_irq_restore(flags);

    memcpy(out, cur->ipc_msg, sizeof(uint64_t) * IPC_MSG_WORDS);
    return cur->ipc_sender;
}
