#include <koharu/bootinfo.h>
#include <koharu/initcall.h>

#include <asm/pmap.h>

int vmm_init() {
    dprintf("kernel src: 0x%lx\n", g_boot_info->kernel.kernel_src);
    dprintf("kernel size: %luB\n", g_boot_info->kernel.kernel_size);

    return 0;
}

core_initcall(vmm_init, 1);