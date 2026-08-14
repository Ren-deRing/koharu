#include <offsets.h>

.code64

.global switch_to
// void switch_to(struct thread *prev, struct thread *next);

switch_to:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    // switch kernel rsp
    movq %rsp, THREAD_ARCH_RSP_OFFSET(%rdi) // prev->arch.rsp = current rsp
    movq THREAD_ARCH_RSP_OFFSET(%rsi), %rsp // current rsp = next->arch.rsp

    // fsgsbase
    rdfsbase %rax
    movq %rax, THREAD_ARCH_FSBASE_OFFSET(%rdi) // prev->arch.fs_base = rdfsbase()
    
    movq THREAD_ARCH_FSBASE_OFFSET(%rsi), %rax
    wrfsbase %rax                              // wrfsbase(next->arch.fs_base)

    // xsaves
    movl $0xFFFFFFFF, %eax
    movl $0xFFFFFFFF, %edx

    movq THREAD_ARCH_XSAVE_OFFSET(%rdi), %rbx // prev->arch.xsaves_area
    xsaves (%rbx)

    movq THREAD_ARCH_XSAVE_OFFSET(%rsi), %rbx // next->arch.xsaves_area
    xrstors (%rbx)

    // switch address space
    movq THREAD_PMAP_OFFSET(%rsi), %rbx
    movq PMAP_ROOT_PHYS_OFFSET(%rbx), %rcx
    movq %cr3, %rdx
    cmpq %rcx, %rdx
    
    je .Lskip_cr3_switch
    movq %rcx, %cr3

.Lskip_cr3_switch:
    leaq THREAD_KSTACK_OFFSET(%rsi), %rax // %rsi = next
    addq $16384, %rax // kstack_size
    movq %rax, %gs:CPU_KSTACK_TOP_OFFSET
    movq %gs:CPU_ARCH_CPU_DATA_OFFSET, %rcx
    movq CPU_ARCH_TSS_OFFSET(%rcx), %rcx
    movq %rax, TSS_RSP0_OFFSET(%rcx)

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    ret

.global switch_to_first
// void switch_to_first(struct thread *next);

switch_to_first:
    movq THREAD_ARCH_RSP_OFFSET(%rdi), %rsp

    movq THREAD_ARCH_FSBASE_OFFSET(%rdi), %rax
    wrfsbase %rax

    movl $0xFFFFFFFF, %eax
    movl $0xFFFFFFFF, %edx
    movq THREAD_ARCH_XSAVE_OFFSET(%rdi), %rbx
    xrstors (%rbx)

    movq THREAD_PMAP_OFFSET(%rdi), %rbx
    movq PMAP_ROOT_PHYS_OFFSET(%rbx), %rcx
    movq %rcx, %cr3

    leaq THREAD_KSTACK_OFFSET(%rdi), %rax // %rdi = next
    addq $16384, %rax // kstack_size
    movq %rax, %gs:CPU_KSTACK_TOP_OFFSET
    movq %gs:CPU_ARCH_CPU_DATA_OFFSET, %rcx
    movq CPU_ARCH_TSS_OFFSET(%rcx), %rcx
    movq %rax, TSS_RSP0_OFFSET(%rcx)

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    ret

.global user_trampoline
user_trampoline:
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

    addq $16, %rsp

    iretq