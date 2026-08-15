#include <offsets.h>

.code64

.global syscall_entry
syscall_entry:
    swapgs              // as you know.
    
    // save user rsp & switch to kstack
    movq %rsp, %gs:CPU_USER_SP_OFFSET
    movq %gs:CPU_KSTACK_TOP_OFFSET, %rsp 

    pushq %r11          // rflags
    pushq %rcx          // rip

    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, %rdi
    call syscall_dispatch

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    popq %rcx           // rip -> rcx
    popq %r11           // rflags -> r11

    // restore user rsp
    movq %gs:CPU_USER_SP_OFFSET, %rsp

    swapgs

    sysretq