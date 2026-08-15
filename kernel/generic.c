#include <koharu/root.h>
#include <koharu/cpu.h>
#include <koharu/initcall.h>
#include <koharu/bootinfo.h>
#include <koharu/sched.h>

boot_info_t* g_boot_info;

__attribute__((section("entry")))
void generic_entry(boot_info_t* boot_info) {
    g_boot_info = boot_info;

    run_initcalls(__early_initcall_start, __early_initcall_end);
    run_initcalls(__core_initcall_start, __core_initcall_end);
    run_initcalls(__arch_initcall_start, __arch_initcall_end);
    run_initcalls(__sys_initcall_start, __sys_initcall_end);
    run_initcalls(__late_initcall_start, __late_initcall_end);

    rootserver_boot();
    sched_boot();

    for (;;) arch_halt();
}
