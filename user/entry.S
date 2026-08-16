.section .entry, "ax", @progbits
.code64

.global _start
_start:
    andq $-16, %rsp
    call root_main
    int3
