#include <uefi.h>
#include <bootinfo.h>

typedef void (*kernel_entry_t)(boot_info_t *boot_info);

void jump_to_kernel(uint64_t kernel_entry_virt, uint64_t root_phys, boot_info_t *boot_info) {
    __asm__ volatile("cli");

    static uint8_t stack[65536] __attribute__((aligned(16)));

    uintptr_t stack_phys_top = (uintptr_t)stack + sizeof(stack);
    uintptr_t stack_virt_top = 0xFFFF800000000000ULL + stack_phys_top;

    __asm__ volatile(
        "mov %0, %%rsp\n\t"
        "mov %1, %%cr3\n\t"
        "jmp *%2\n\t"
        :
        : "r"(stack_virt_top),
          "r"(root_phys),
          "r"(kernel_entry_virt),
          "D"(boot_info)
        : "memory"
    );

    while (1) {
        __asm__ volatile("hlt");
    }
}
