#pragma once

typedef int (*initcall_t)(void);

#define __define_initcall(section_name, level, fn) \
    static initcall_t __initcall_##level##_##fn \
    __attribute__((used, section("." #section_name "." #level))) = fn

#define early_initcall(fn, prio)          __define_initcall(early_initcall, prio, fn)
#define core_initcall(fn, prio)           __define_initcall(core_initcall, prio, fn)
#define arch_initcall(fn, prio)           __define_initcall(arch_initcall, prio, fn)
#define sys_initcall(fn, prio)            __define_initcall(sys_initcall, prio, fn)
#define late_initcall(fn, prio)           __define_initcall(late_initcall, prio, fn)

extern initcall_t __early_initcall_start[], __early_initcall_end[];
extern initcall_t __core_initcall_start[],  __core_initcall_end[];
extern initcall_t __arch_initcall_start[],  __arch_initcall_end[];
extern initcall_t __sys_initcall_start[],   __sys_initcall_end[];
extern initcall_t __late_initcall_start[],  __late_initcall_end[];

static inline void run_initcalls(initcall_t* start, initcall_t* end) {
    for (initcall_t* fn = start; fn < end; fn++) {
        if (!fn || !*fn) continue;

        int ret = (*fn)();
        // if (ret != 0) {
        //     kprintf("\n[initcall] function at %p failed with code %d\n", *fn, ret);
        //     kpanic("kernel initialization failed");
        // }
    }
}