#include <koharu/initcall.h>
#include <koharu/print.h>

int init_idt() {
    dprintf("Hello from IDT init!\n");
    dprintf("But i have no idea to initialize idt!\n");
    dprintf("So, i'll panic this kernel!\n");

    return -1;
}

early_initcall(init_idt, B1);