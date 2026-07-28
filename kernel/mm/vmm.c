#include <koharu/bootinfo.h>
#include <koharu/initcall.h>

#include <asm/pmap.h>

int vmm_init() {

    return 0;
}

core_initcall(vmm_init, 1);