#include <koharu/initcall.h>
#include <koharu/bootinfo.h>
#include <koharu/print.h>

__attribute__((section("entry")))
void generic_entry(boot_info_t* boot_info) {
    volatile uint32_t* fb = boot_info->screen.fb;
    uint32_t total_pixels = (boot_info->screen.pitch / 4) * boot_info->screen.height;
    
    for (uint32_t i = 0; i < total_pixels; i++) {
        fb[i] = 0xCC9BA3; // KOHARU
    }

    run_initcalls(__early_initcall_start, __early_initcall_end);

    dprintf("Hello World!\n");
    dprintf("This is kernel speaking. How are you?\n");
    dprintf("Uh, anyway, early initcall start addr is 0x%lx\n", __early_initcall_start);
    dprintf("Have a good day!\n");

    for (;;) asm __volatile__ ("hlt");
}