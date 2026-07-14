.code16

.text
.global _enter

_enter:
    in $0x92, %al  # read system control port A
    or $2, %al     # bit 1: A20 gate enable
    out %al, $0x92 # write back

    cli
    lgdt (_gdtr)   # load gdt
    mov %cr0, %eax # read cr0
    or  $1, %al    # bit 1: PE (enable protected mode)
    mov %eax, %cr0 # write back

    ljmp $0x08, $_start32 # SS (RPL(2b), TI(1b), GDT IDX(13b)) so 0x08 => 0000 0000 0000 1000 (Ring 0, Use GDT, GDT Idx 1)

.code32
_start32:
    mov $0x10, %ax # same! 0x10 => 0000 0000 0001 0000, GDT Idx 2
    mov %ax, %ds   # ds, es to 0x10
    mov %ax, %es
    mov %ax, %ss

    jmp . # hlt for now.

# Segment Descriptor
# BASE(1B) FLAGS(4b) LIMIT(4b) ACCBYTE(1B) BASE(1B)
# BASE(2B) LIMIT(2B)

# P: Present, DPL(2b): Privilege Level, E: Execution
# D/C: Direction / Conforming, RW: Readable / Writeable, A: Is CPU Accessed?

# G: Granularity (0: 1B, 1: 4KiB), DB: Size (0: 16, 1: 32 [bits]), L: Long Mode, Reserved

_gdt:
    .quad 0

    .long 0x0000FFFF # Base: 0000 Limit: FFFF (4GB)
    .long 0x00CF9A00 # (1(G)1(DB)0(L)0(X) = C) (1(P)00(DPL)1(S)1(E)0(C)1(RW)0(A) = 9A)

    .long 0x0000FFFF
    .long 0x00CF9200 # (1(P)00(DPL)1(S)0(E)0(D)1(RW)0(A) = 92)
_gdt.end:

_gdtr:
    .word (_gdt.end - _gdt) - 1 # GDT is always bigger than 1 Byte.
    .long _gdt                  # GDT's addr