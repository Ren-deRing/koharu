#include <koharu/cpu.h>
#include <koharu/grant.h>
#include <koharu/mmu.h>
#include <koharu/pmap.h>
#include <koharu/print.h>
#include <koharu/sched.h>
#include <koharu/thread.h>
#include <koharu/syscall.h>

#include <stddef.h>
#include <stdint.h>

uint64_t do_syscall(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, struct syscall_ret *ret) {
    switch (num) {
        case SYS_EXIT: {
            thread_exit();
            break;
        }
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
        case SYS_MAP: {
            struct thread *cur    = curcpu->current;
            struct thread *target = thread_lookup(a1);
            if (!target) return (uint64_t)-1;
            if (a1 != cur->tid && cur->pager_tid != a1) return (uint64_t)-1;
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
        case SYS_UNMAP: {
            struct thread *cur    = curcpu->current;
            struct thread *target = thread_lookup(a1);
            if (!target) return (uint64_t)-1;
            if (a1 != cur->tid && cur->pager_tid != a1) return (uint64_t)-1;
            if (a3 == 0) return (uint64_t)-1;

            for (uintptr_t va = a2 & ~0xFFFULL; va < a2 + a3; va += PAGE_SIZE) {
                uintptr_t phys = pmap_extract(target->pmap, va);
                if (!phys) continue;
                pmap_unmap(target->pmap, va, PAGE_SIZE);
                grant_mapping_remove(phys_to_pfn(phys));
            }

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
        case SYS_CREATE: {
            struct thread *cur = curcpu->current;

            pmap_t *pmap = pmap_create();
            if (!pmap) return (uint64_t)-1;

            struct thread *t = thread_create_child(pmap, 0);
            if (!t) {
                pmap_destroy(pmap);
                return (uint64_t)-1;
            }

            t->pager_tid = cur->tid;

            return t->tid;
        }
        case SYS_START: {
            struct thread *cur    = curcpu->current;
            struct thread *target = thread_lookup(a1);
            if (!target) return (uint64_t)-1;
            if (target->pager_tid != cur->tid) return (uint64_t)-1;
            if (target->state != THREAD_CREATED) return (uint64_t)-1;

            target->tf->rip = a2;
            target->tf->rdi = a3;
            sched_enqueue(target);

            return 0;
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
