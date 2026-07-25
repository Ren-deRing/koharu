#pragma once

#include <koharu/cpu.h>
#include <koharu/print.h>

typedef int (*initcall_t)(void);

typedef struct {
    initcall_t fn;
    const char* name;
} initcall_entry_t;

#define __define_initcall(section_name, level, func) \
    static initcall_entry_t __initcall_##level##_##func \
    __attribute__((used, section("." #section_name "." #level))) = { .fn = func, .name = #func }

#define early_initcall(fn, prio)          __define_initcall(early_initcall, prio, fn)
#define core_initcall(fn, prio)           __define_initcall(core_initcall, prio, fn)
#define arch_initcall(fn, prio)           __define_initcall(arch_initcall, prio, fn)
#define sys_initcall(fn, prio)            __define_initcall(sys_initcall, prio, fn)
#define late_initcall(fn, prio)           __define_initcall(late_initcall, prio, fn)

extern initcall_entry_t __early_initcall_start[], __early_initcall_end[];
extern initcall_entry_t __core_initcall_start[],  __core_initcall_end[];
extern initcall_entry_t __arch_initcall_start[],  __arch_initcall_end[];
extern initcall_entry_t __sys_initcall_start[],   __sys_initcall_end[];
extern initcall_entry_t __late_initcall_start[],  __late_initcall_end[];

static inline void run_initcalls(initcall_entry_t* start, initcall_entry_t* end) {
    for (initcall_entry_t* entry = start; entry < end; entry++) {
        if (!entry->fn || !*entry->fn) continue;

        dprintf("%s... ", entry->name);

        int ret = (*entry->fn)();
        if (ret != 0) {
            dprintf("failed.\n");
            dprintf("initcall panic! code: %d\n", ret);
            for (;;) arch_halt();
        }

        dprintf("ok\n");
    }
}