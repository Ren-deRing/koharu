#include <koharu/cpu.h>
#include <koharu/futex.h>
#include <koharu/grant.h>
#include <koharu/mmu.h>
#include <koharu/pmap.h>
#include <koharu/print.h>
#include <koharu/sched.h>
#include <koharu/thread.h>
#include <koharu/syscall.h>

#include <asm/usermem.h>

#include <stddef.h>
#include <stdint.h>

#define IPC_OP(x)       ((x) >> 32)
#define IPC_TARGET(x)   ((x) & 0xFFFFFFFF)

static uint64_t sys_thread_control(uint64_t op, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    struct thread *cur = curcpu->current;

    switch (op) {
    case TC_CREATE: {
        pmap_t *pmap = a3 ? (pmap_t *)a3 : pmap_create();
        if (!pmap) return (uint64_t)-1;

        struct thread *t = thread_create_child(pmap, 0);
        if (!t) {
            if (!a3) pmap_destroy(pmap);
            return (uint64_t)-1;
        }

        t->pager_tid = (uint64_t)a4;
        t->utcb      = (uintptr_t)a5;

        if (t->utcb) {
            put_user_u64((uint64_t *)(t->utcb + 0), t->tid);
            put_user_u64((uint64_t *)(t->utcb + 8), t->pager_tid);
            t->tf->rsi = t->utcb;  // crt0.S reads utcb from rsi
        }

        return t->tid;
    }
    case TC_DESTROY: {
        struct thread *target = thread_lookup(a2);
        if (!target) return (uint64_t)-1;
        if (target->pager_tid != cur->tid) return (uint64_t)-1;

        if (target->state == THREAD_READY || target->state == THREAD_RUNNING)
            sched_exit();

        thread_destroy(target);
        return 0;
    }
    case TC_BIND_SPACE: {
        struct thread *target = thread_lookup(a2);
        if (!target) return (uint64_t)-1;
        if (target->pager_tid != cur->tid) return (uint64_t)-1;
        if (!a3) return (uint64_t)-1;

        target->pmap = (pmap_t *)a3;
        return 0;
    }
    default:
        return (uint64_t)-1;
    }
}

static uint64_t sys_exchange_registers(uint64_t target_tid, uint64_t ip_ptr, uint64_t sp_ptr, uint64_t flags_ptr, uint64_t op) {
    struct thread *target = thread_lookup(target_tid);
    if (!target) return (uint64_t)-1;

    switch (op) {
    case EXR_READ: {
        if (ip_ptr) put_user_u64((uint64_t *)ip_ptr, target->tf->rip);
        if (sp_ptr) put_user_u64((uint64_t *)sp_ptr, target->tf->rsp);
        if (flags_ptr) put_user_u64((uint64_t *)flags_ptr, target->tf->rflags);
        return 0;
    }
    case EXR_WRITE: {
        uint64_t val;
        if (ip_ptr) { get_user_u64((uint64_t *)ip_ptr, &val); target->tf->rip = val; }
        if (sp_ptr) { get_user_u64((uint64_t *)sp_ptr, &val); target->tf->rsp = val; }
        if (flags_ptr) { get_user_u64((uint64_t *)flags_ptr, &val); target->tf->rflags = val; }
        return 0;
    }
    case EXR_ACTIVATE: {
        if (target->state != THREAD_CREATED) return (uint64_t)-1;
        sched_enqueue(target);
        return 0;
    }
    case EXR_DEACTIVATE: {
        if (target->state == THREAD_RUNNING) return (uint64_t)-1;
        if (target->state == THREAD_READY) {
            sched_dequeue(target);
        }
        target->state = THREAD_CREATED;
        return 0;
    }
    case EXR_SET_ENTRY: {
        target->tf->rip = ip_ptr;
        target->tf->rdi = sp_ptr;
        target->tf->rsi = flags_ptr;
        return 0;
    }
    default:
        return (uint64_t)-1;
    }
}

static uint64_t sys_ipc(uint64_t packed, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, struct syscall_ret *ret) {
    uint64_t op    = IPC_OP(packed);
    uint64_t tid   = IPC_TARGET(packed);
    uint64_t words[IPC_MSG_WORDS] = { a2, a3, a4, a5, a6 };
    uint64_t out[IPC_MSG_WORDS];

    switch (op) {
    case IPC_SEND: {
        struct thread *target = thread_lookup(tid);
        if (!target) return (uint64_t)-1;
        return ipc_send(target, words);
    }
    case IPC_RECV: {
        uint64_t sender = ipc_recv(out);
        for (int i = 0; i < IPC_MSG_WORDS; i++) ret->extra[i] = out[i];
        return sender;
    }
    case IPC_CALL: {
        struct thread *target = thread_lookup(tid);
        if (!target) return (uint64_t)-1;
        uint64_t sender = ipc_call(target, words, out);
        for (int i = 0; i < IPC_MSG_WORDS; i++) ret->extra[i] = out[i];
        return sender;
    }
    case IPC_REPLY_AND_WAIT: {
        uint64_t sender = ipc_reply_recv(words, out);
        for (int i = 0; i < IPC_MSG_WORDS; i++) ret->extra[i] = out[i];
        return sender;
    }
    default:
        return (uint64_t)-1;
    }
}

uint64_t do_syscall(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, struct syscall_ret *ret) {
    switch (num) {
    case SYS_THREAD_CONTROL:
        return sys_thread_control(a1, a2, a3, a4, a5);

    case SYS_EXCHANGE_REGISTERS:
        return sys_exchange_registers(a1, a2, a3, a4, a5);

    case SYS_IPC:
        return sys_ipc(a1, a2, a3, a4, a5, a6, ret);

    case SYS_MAP: {
        struct thread *cur    = curcpu->current;
        struct thread *target = thread_lookup(a1);
        if (!target) return (uint64_t)-1;
        if (target->pager_tid != cur->tid) return (uint64_t)-1;
        if (a4 == 0) return (uint64_t)-1;

        uint8_t rights = 0;
        if (a5 & PROT_READ)  rights |= GRANT_READ;
        if (a5 & PROT_WRITE) rights |= GRANT_WRITE;
        if (rights == 0) return (uint64_t)-1;

        for (uintptr_t pa = a3; pa < a3 + a4; pa += PAGE_SIZE) {
            if (!grant_holds(cur->pmap, phys_to_pfn(pa), rights)) return (uint64_t)-1;
        }

        if (pmap_map(target->pmap, a2, a3, a4, a5 | PROT_USER) != 0) return (uint64_t)-1;

        for (uintptr_t pa = a3; pa < a3 + a4; pa += PAGE_SIZE)
            grant_mapping_add(phys_to_pfn(pa));

        return 0;
    }

    case SYS_GRANT: {
        struct thread *cur    = curcpu->current;
        struct thread *target = thread_lookup(a1);
        if (!target) return (uint64_t)-1;
        if (a3 == 0 || (a3 & ~(GRANT_READ | GRANT_WRITE | GRANT_GRANT))) return (uint64_t)-1;

        uint64_t pfn = phys_to_pfn(a2);
        if (!grant_holds(cur->pmap, pfn, a3)) return (uint64_t)-1;
        if (grant_add(target->pmap, pfn, a3) != 0) return (uint64_t)-1;

        return 0;
    }

    case SYS_UNMAP: {
        struct thread *cur    = curcpu->current;
        struct thread *target = thread_lookup(a1);
        if (!target) return (uint64_t)-1;
        if (target->pager_tid != cur->tid) return (uint64_t)-1;
        if (a3 == 0) return (uint64_t)-1;

        for (uintptr_t va = a2 & ~0xFFFULL; va < a2 + a3; va += PAGE_SIZE) {
            uintptr_t phys = pmap_extract(target->pmap, va);
            if (!phys) continue;
            pmap_unmap(target->pmap, va, PAGE_SIZE);
            grant_mapping_remove(phys_to_pfn(phys));
        }

        return 0;
    }

    case SYS_SCHEDULE: {
        struct thread *target = thread_lookup(a1);
        if (!target) return (uint64_t)-1;
        if (a2 != (uint64_t)-1) target->current_level = (uint32_t)a2;
        if (a3 != (uint64_t)-1) target->time_quantum_left = (uint32_t)a3;
        return 0;
    }

    case SYS_THREAD_SWITCH:
        sched_yield();
        return 0;

    case SYS_WRITE: {
        struct thread *cur = curcpu->current;

        if (a1 != 1 && a1 != 2) return (uint64_t)-1;
        if (a3 == 0) return 0;

        for (size_t off = 0; off < a3;) {
            uintptr_t phys = pmap_extract(cur->pmap, a2 + off);
            if (!phys) return (uint64_t)-1;

            uint8_t *page     = (uint8_t *)phys_to_virt(phys & ~0xFFFULL);
            size_t page_off   = (a2 + off) & 0xFFF;
            size_t chunk      = PAGE_SIZE - page_off;
            if (chunk > a3 - off) chunk = a3 - off;

            for (size_t i = 0; i < chunk; i++) kputc(page[page_off + i]);

            off += chunk;
        }

        return a3;
    }

    case SYS_FUTEX:
        if (a2 == FUTEX_WAIT)
            return (uint64_t)futex_wait((void *)a1, (uint32_t)a3);
        else if (a2 == FUTEX_WAKE)
            return (uint64_t)futex_wake((void *)a1, (int)a3);
        return (uint64_t)-1;

    case SYS_PROCESSOR_CONTROL:
    case SYS_EXCEPTION_HANDLER:
    case SYS_SYSTEM_CONTROL:
    case SYS_SPACE_CONTROL:
        return (uint64_t)-1;

    default:
        break;
    }

    return 0;
}
