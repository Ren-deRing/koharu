.code16

.text
.global _stage2

_stage2:
_ready.protected:
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
_start32:          # initalize segments
    mov $0x10, %ax # same! 0x10 => 0000 0000 0001 0000, GDT Idx 2
    mov %ax, %ds   # segments to 0x10
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs

    mov $0xFFFE, %esp
    mov $0xFFFE, %ebp

.equ PML4T_ADDR, 0x1000
.equ PDPT_ADDR, 0x2000
.equ PDT_ADDR, 0x3000
.equ PT_ADDR, 0x4000
.equ SIZEOF_PAGE_TABLE, 4096

.equ PT_ADDR_MASK, 0xFFFFFFFFFF000
.equ PT_PRESENT, 1
.equ PT_READABLE, 2

.equ ENTRIES_PER_PT, 512
.equ SIZEOF_PT_ENTRY, 8
.equ PAGE_SIZE, 0x1000

.equ CR4_PAE_ENABLE, 1 << 5
.equ EFER_MSR, 0xC0000080
.equ EFER_LM_ENABLE, 1 << 8
.equ CR0_PM_ENABLE, 1 << 0
.equ CR0_PG_ENABLE, 1 << 31

_setup.paging:
    mov $PML4T_ADDR, %edi # memset target
    mov %edi, %cr3        # cr3 knows the addr of the page table

    xor %eax, %eax               # memset value (0)
    mov $SIZEOF_PAGE_TABLE, %ecx # 'll repeat 4096 times
    rep stosl                    # fill 4B zero to edi for ecx times (clearing 16KB for pages)
    mov %cr3, %edi               # back to PML4T ADDR

    movl $(PDPT_ADDR  & PT_ADDR_MASK | PT_PRESENT | PT_READABLE), (%edi) # write PML4T (PDPT's phys addr, flags)

    mov $PDPT_ADDR, %edi
    movl $(PDT_ADDR & PT_ADDR_MASK | PT_PRESENT | PT_READABLE), (%edi)   # write PDPT  (same but PDT's phys addr)

    mov $PDT_ADDR, %edi
    movl $(PT_ADDR & PT_ADDR_MASK | PT_PRESENT | PT_READABLE), (%edi)    # write PDT   (too)

    mov $PT_ADDR, %edi                    # page table's addr (start addr: PT_ADDR)
    mov $(PT_PRESENT | PT_READABLE), %ebx # page table content (0x00000003)
    mov $ENTRIES_PER_PT, %ecx             # full page table (2MiB)

_set.entry:
    movl %ebx, (%edi)          # write entry
    add $PAGE_SIZE, %ebx       # next phys addr (0x00000003 + 0x1000 = 0x00001003)
    add $SIZEOF_PT_ENTRY, %edi # move to next page table entry
    loop _set.entry            # loop, ecx--

_ready.long:
    mov %cr4, %eax            # read cr4
    or $CR4_PAE_ENABLE, %eax  # enable PAE
    mov %eax, %cr4            # write back

    mov $EFER_MSR, %ecx       # extended feature enable register's msr
    rdmsr                     # read!
    or $EFER_LM_ENABLE, %eax  # enable long mode
    wrmsr                     # write back

    mov %cr0, %eax           # read cr0
    or $CR0_PG_ENABLE, %eax  # set PG bit (paging enable)
    mov %eax, %cr0           # write back 

_jmp.long:
    ljmp $0x18, $_start64 # Ring 0, Use GDT, GDT Idx 3

.code64
_start64:
    xor %ax, %ax # long mode doesn't use segmentation
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs

    mov $0x1FFFF0, %rsp
    mov $0x1FFFF0, %rbp

    .extern __bss_start
    .extern __bss_end

    lea __bss_start, %rdi # load absolute addresses
    lea __bss_end, %rax

    sub %rdi, %rax  # __bss_end - __bss_start = __bss_size
    shr $3, %rax    # __bss_size / 8 = loop count

    test %rax, %rax # is __bss_size 0?
    jz _call.c      # skip bss clear

    mov %rax, %rcx # loop count
    xor %rax, %rax # memset value (0)
    rep stosq      # fill 8B zero to __bss_start for rcx times

_call.c:
    .extern loader_entry
    call loader_entry

_hlt.loop: # how?
    cli
    hlt
    jmp _hlt.loop

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

    # long mode
    .long 0xC0FA1580 # KOHA(RU)158(CM)
    .long 0x00209A00 # (0(G)0(DB)1(L)0(X) = 2) (1(P)00(DPL)1(S)1(E)0(C)1(RW)0(A) = 9A)
_gdt.end:

_gdtr:
    .word (_gdt.end - _gdt) - 1 # GDT is always bigger than 1 Byte.
    .long _gdt                  # GDT's addr