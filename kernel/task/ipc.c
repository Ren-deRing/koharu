#include <koharu/list.h>
#include <koharu/cpu.h>
#include <koharu/lock.h>
#include <koharu/sched.h>
#include <koharu/thread.h>
#include <koharu/ipc.h>
#include <string.h>

/*
 * IPC is protected by a per-endpoint spinlock (ep->lock).
 *
 * A thread's ipc_ep, ipc_msg and ipc_sender are only mutated while holding
 * that thread's ep->lock.  A thread never holds more than one ep->lock at a
 * time, and never blocks while holding one, so there is no lock-order cycle.
 *
 * Two list nodes are used:
 *   - ipc_list      : sender slot on someone's endpoint queue (or the server
 *                     enqueuing itself while waiting for a request).
 *   - ipc_wait_list : a caller's own reply slot, so a later reply can find
 *                     and wake it even when the caller also sits on the
 *                     target's sender queue.
 *
 * Replies are matched per call. Every ipc_call takes a monotonically increasing seq on the caller,
 * the receiver records it as ipc_prev_seq.
 * ipc_reply_recv only delivers a reply when the caller is blocked on exactly that seq,
 * so a stale reply can never be delivered to a newer call.
 * Reply delivery never touches the receiver's ipc_prev_seq,
 * and ipc_call saves/restores the caller's ipc_sender,
 * so a server can make nested calls to other servers without losing reply context.
 */

static void ipc_deliver_msg(struct thread *recv, uint64_t sender_tid, const uint64_t *words,
                            uint64_t prev_seq) {
    // recv->ipc_ep.lock must be held
    struct ipc_endpoint *ep = &recv->ipc_ep;

    list_del(ep->ipc_waitqueue.next);

    memcpy(recv->ipc_msg, words, sizeof(uint64_t) * IPC_MSG_WORDS);
    recv->ipc_sender = sender_tid;
    recv->ipc_prev_seq = prev_seq;

    if (list_empty((list_head *)&ep->ipc_waitqueue))
        ep->state = IPC_EP_INACTIVE;

    if (recv->state == THREAD_BLOCKED)
        sched_wakeup(recv);
}

static void ipc_deliver_reply(struct thread *recv, uint64_t sender_tid, const uint64_t *words) {
    // recv->ipc_ep.lock must be held; unlike a new message this must not touch recv->ipc_prev_seq
    struct ipc_endpoint *ep = &recv->ipc_ep;

    list_del(ep->ipc_waitqueue.next);

    memcpy(recv->ipc_msg, words, sizeof(uint64_t) * IPC_MSG_WORDS);
    recv->ipc_sender = sender_tid;

    if (list_empty((list_head *)&ep->ipc_waitqueue))
        ep->state = IPC_EP_INACTIVE;

    if (recv->state == THREAD_BLOCKED)
        sched_wakeup(recv);
}

static uint64_t ipc_accept(struct thread *recv, uint64_t out[IPC_MSG_WORDS]) {
    // recv->ipc_ep.lock must be held
    struct ipc_endpoint *ep = &recv->ipc_ep;

    struct thread *snd = container_of(ep->ipc_waitqueue.next, struct thread, ipc_list);
    uint64_t sender_tid = snd->tid;

    list_del(&snd->ipc_list);
    memcpy(recv->ipc_msg, snd->ipc_msg, sizeof(uint64_t) * IPC_MSG_WORDS);
    recv->ipc_sender = sender_tid;
    recv->ipc_prev_seq = (snd->ipc_ep.state == IPC_EP_WAIT_RECV) ? snd->ipc_call_seq : 0;

    if (list_empty((list_head *)&ep->ipc_waitqueue))
        ep->state = IPC_EP_INACTIVE;

    // a caller stays blocked.. a plain sender is done
    if (snd->ipc_ep.state != IPC_EP_WAIT_RECV && snd->state == THREAD_BLOCKED)
        sched_wakeup(snd);

    if (out)
        memcpy(out, recv->ipc_msg, sizeof(uint64_t) * IPC_MSG_WORDS);

    return sender_tid;
}

int ipc_send(struct thread *target, const uint64_t words[IPC_MSG_WORDS]) {
    struct thread *cur = curcpu->current;
    struct ipc_endpoint *ep = &target->ipc_ep;

    cpu_status_t flags = arch_irq_save();

    spin_lock(&ep->lock);

    if (ep->state == IPC_EP_WAIT_RECV) {
        // target is waiting on its own queue, deliver straight to it
        ipc_deliver_msg(target, cur->tid, words, 0);
        spin_unlock(&ep->lock);
        arch_irq_restore(flags);
        return 0;
    }

    memcpy(cur->ipc_msg, words, sizeof(uint64_t) * IPC_MSG_WORDS);
    ep->state = IPC_EP_WAIT_SEND;
    list_add_tail(&cur->ipc_list, (list_head *)&ep->ipc_waitqueue);

    spin_unlock(&ep->lock);

    sched_block();
    arch_irq_restore(flags);
    return 0;
}

uint64_t ipc_recv(uint64_t words[IPC_MSG_WORDS]) {
    struct thread *cur = curcpu->current;
    struct ipc_endpoint *ep = &cur->ipc_ep;

    cpu_status_t flags = arch_irq_save();

    spin_lock(&ep->lock);

    if (ep->state == IPC_EP_WAIT_SEND) {
        uint64_t sender = ipc_accept(cur, words);
        spin_unlock(&ep->lock);
        arch_irq_restore(flags);
        return sender;
    }

    ep->state = IPC_EP_WAIT_RECV;
    list_add_tail(&cur->ipc_list, (list_head *)&ep->ipc_waitqueue);

    spin_unlock(&ep->lock);

    sched_block();

    spin_lock(&ep->lock);
    memcpy(words, cur->ipc_msg, sizeof(uint64_t) * IPC_MSG_WORDS);
    uint64_t sender = cur->ipc_sender;
    spin_unlock(&ep->lock);

    arch_irq_restore(flags);
    return sender;
}

uint64_t ipc_call(struct thread *target, const uint64_t in[IPC_MSG_WORDS], uint64_t out[IPC_MSG_WORDS]) {
    struct thread *cur = curcpu->current;
    struct ipc_endpoint *myep = &cur->ipc_ep;
    struct ipc_endpoint *tep = &target->ipc_ep;

    cpu_status_t flags = arch_irq_save();

    cur->ipc_call_seq++;
    uint64_t seq = cur->ipc_call_seq;

    // a server that makes a nested call
    uint64_t saved_sender;
    spin_lock(&myep->lock);
    saved_sender = cur->ipc_sender;
    // reserve a reply slot on our own endpoint
    myep->state = IPC_EP_WAIT_RECV;
    list_add_tail(&cur->ipc_wait_list, (list_head *)&myep->ipc_waitqueue);
    spin_unlock(&myep->lock);

    // hand the message over to the target
    spin_lock(&tep->lock);
    if (tep->state == IPC_EP_WAIT_RECV) {
        ipc_deliver_msg(target, cur->tid, in, seq);
    } else {
        memcpy(cur->ipc_msg, in, sizeof(uint64_t) * IPC_MSG_WORDS);
        tep->state = IPC_EP_WAIT_SEND;
        list_add_tail(&cur->ipc_list, (list_head *)&tep->ipc_waitqueue);
    }
    spin_unlock(&tep->lock);

    sched_block();

    spin_lock(&myep->lock);
    memcpy(out, cur->ipc_msg, sizeof(uint64_t) * IPC_MSG_WORDS);
    uint64_t sender = cur->ipc_sender;
    cur->ipc_sender = saved_sender;
    spin_unlock(&myep->lock);

    arch_irq_restore(flags);
    return sender;
}

uint64_t ipc_reply_recv(const uint64_t reply[IPC_MSG_WORDS], uint64_t out[IPC_MSG_WORDS]) {
    struct thread *cur = curcpu->current;
    struct ipc_endpoint *ep = &cur->ipc_ep;

    cpu_status_t flags = arch_irq_save();

    // reply to the prev caller
    uint64_t prev_sender;
    spin_lock(&ep->lock);
    prev_sender = cur->ipc_sender;
    spin_unlock(&ep->lock);

    struct thread *caller = thread_lookup(prev_sender);
    if (cur->ipc_prev_seq != 0 && caller && caller != cur &&
        caller->ipc_call_seq == cur->ipc_prev_seq) {
        spin_lock(&caller->ipc_ep.lock);
        if (caller->ipc_ep.state == IPC_EP_WAIT_RECV &&
            !list_empty((list_head *)&caller->ipc_ep.ipc_waitqueue)) {
            ipc_deliver_reply(caller, cur->tid, reply);
        }
        spin_unlock(&caller->ipc_ep.lock);
    }

    // wait for the next request (or accept one that is already pending)
    spin_lock(&ep->lock);

    if (ep->state == IPC_EP_WAIT_SEND) {
        uint64_t sender = ipc_accept(cur, out);
        spin_unlock(&ep->lock);
        arch_irq_restore(flags);
        return sender;
    }

    ep->state = IPC_EP_WAIT_RECV;
    list_add_tail(&cur->ipc_list, (list_head *)&ep->ipc_waitqueue);

    spin_unlock(&ep->lock);

    sched_block();

    spin_lock(&ep->lock);
    memcpy(out, cur->ipc_msg, sizeof(uint64_t) * IPC_MSG_WORDS);
    uint64_t sender = cur->ipc_sender;
    spin_unlock(&ep->lock);

    arch_irq_restore(flags);
    return sender;
}
