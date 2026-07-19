.code16

.text
.global _stage2

_stage2:
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es

_do.mmap:
    mov $0xE820, %eax      # let's do e820

    mov $0x8000, %edi      # our mmap will be stored to this addr.
    xor %ebx, %ebx
    mov $0x0534D4150, %edx # signature, "SMAP"
    movl $1, %es:20(%di)   # request ACPI 3.x+ entry
    mov $24, %ecx          # result buffer size
    
    int $0x15
    jc _panic.mmap         # carry flag set: something went wrong!

    mov $0x0534D4150, %edx # if it succeeded, e820 sets edx to "SMAP" signature.
    cmp %edx, %eax         # no "SMAP"? something went wrong!
    jne _panic.mmap

    test %ebx, %ebx        # ebx = continuation value for next entry, so 0: length 1 -> useless!
    je _panic.mmap

_get.mmap:
    mov $0xE820, %eax      # S
    mov $0x0534D4150, %edx # A
    movl $1, %es:20(%di)   # M
    mov $24, %ecx          # E
    
    int $0x15
    jc _mmap.end           # carry flag set: in this time, means done.
    mov $0x0534D4150, %edx # restoration just in case

_mmap.loop:
    jcxz _skip.entry       # jmp if cx is zero. cx = actual length returned in bytes, so 0: empty!
    cmp $20, %cl           # if response is ACPI 3.x+?
    jbe _no.acpi3          # oh no
    testb $1, %es:20(%di)  # if 'ignore this data' bit set?
    je _skip.entry

_no.acpi3:
    mov %es:8(%di), %ecx   # get lower 32bits of length
    test %ecx, %ecx        # if length > 0?
    jne _good.entry        # good.
    mov %es:12(%di), %ecx  # get higher 32bits of length
    jecxz _skip.entry      # no length? skips.

_good.entry:
    inc %bp                # got it! count++
    add $32, %di           # pad to 32 bytes

_skip.entry:
    test %ebx, %ebx        # if ebx is 0, mmap is completed
    jne _get.mmap

_mmap.end:
    mov %bp, (0x6FFF)      # save length
    xor %eax, %eax         # blank record
	mov $8, %ecx
	rep stosl

_do.vbe:
    lea vbe_modes, %si         # get vbe_modes array address

_vbe.loop:
    mov (%si), %cx             # get item!
    test %cx, %cx              # is item 0? (terminator)
    jz _panic.vbe              # no more tries

    mov $0x4F01, %ax           # 4F01: get mode info
    mov $VBEModeInfoBlock, %di # vbe block buffer addr
    
    int $0x10                  # get!

    cmp $0x004F, %ax           # is this good?
    je _good.mode              # yippee

    add $2, %si                # oh no, we should try again.
    jmp _vbe.loop

_good.mode:
    mov (%si), %bx             # get good mode item
    
    mov $0x4F02, %ax           # 4F02: set video mode
    int $0x10

    cmp $0x004F, %ax           # did it work?
    jne _panic.vbe             # oh.

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

_panic.mmap:
    mov $mmap_panic, %si
    jmp _panic.print

_panic.vbe:
    mov $vbe_panic, %si
    jmp _panic.print

_panic.print:
    call 0x7DF4 # shared _print
    call 0x7DF7 # shared _print.newline

    jmp . # hlt (panic)

.code32
_start32:          # initalize segments
    mov $0x10, %ax # same! 0x10 => 0000 0000 0001 0000, GDT Idx 2
    mov %ax, %ds   # segments to 0x10
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs

    mov $0x7BF0, %esp
    mov $0x7BF0, %ebp

.equ PML4T_ADDR, 0x1000
.equ PDPT_ADDR, 0x2000
.equ PDT_ADDR, 0x3000
.equ PT_ADDR, 0x4000
.equ SIZEOF_PAGE_TABLE, 4096

.equ PT_ADDR_MASK, 0xFFFFFFFFFF000
.equ PT_PRESENT, 1
.equ PT_WRITABLE, 2

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

    movl $(PDPT_ADDR  & PT_ADDR_MASK | PT_PRESENT | PT_WRITABLE), (%edi) # write PML4T (PDPT's phys addr, flags)

    mov $PDPT_ADDR, %edi
    movl $(PDT_ADDR & PT_ADDR_MASK | PT_PRESENT | PT_WRITABLE), (%edi)   # write PDPT  (same but PDT's phys addr)

    mov $PDT_ADDR, %edi
    movl $(PT_ADDR & PT_ADDR_MASK | PT_PRESENT | PT_WRITABLE), (%edi)    # write PDT   (too)

    mov $PT_ADDR, %edi                    # page table's addr (start addr: PT_ADDR)
    mov $(PT_PRESENT | PT_WRITABLE), %ebx # page table content (0x00000003)
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

    mov $0x7C00, %rsp
    mov $0x7C00, %rbp

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
    and $0xFFFFFFFFFFFFFFF0, %rsp
    sub $8, %rsp # 16-Byte alignment (call pushes 8B return address)

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
    .long 0xC0FA1480 # KOHA(RU)148(CM)
    .long 0x00209A00 # (0(G)0(DB)1(L)0(X) = 2) (1(P)00(DPL)1(S)1(E)0(C)1(RW)0(A) = 9A)
_gdt.end:

_gdtr:
    .word (_gdt.end - _gdt) - 1 # GDT is always bigger than 1 Byte.
    .long _gdt                  # GDT's addr

mmap_panic:
    .ascii "mmap panic!"
    .byte 0

vbe_panic:
    .ascii "vbe panic!"
    .byte 0

# VBE

vbe_modes:
    .word 0x4144
    .word 0x4143
    .word 0x4145
    .word 0x0000

.equ VBEModeInfoBlock, 0x5F00

# Mandatory information for all VBE revisions
.equ VBEModeInfoBlock.ModeAttributes,      VBEModeInfoBlock + 0	    # WORD - mode attributes
.equ VBEModeInfoBlock.WinAAttributes,      VBEModeInfoBlock + 2	    # BYTE - window A attributes
.equ VBEModeInfoBlock.WinBAttributes,      VBEModeInfoBlock + 3	    # BYTE - window B attributes
.equ VBEModeInfoBlock.WinGranularity,      VBEModeInfoBlock + 4	    # WORD - window granularity in KB
.equ VBEModeInfoBlock.WinSize,             VBEModeInfoBlock + 6	    # WORD - window size in KB
.equ VBEModeInfoBlock.WinASegment,         VBEModeInfoBlock + 8     # WORD - window A start segment
.equ VBEModeInfoBlock.WinBSegment,         VBEModeInfoBlock + 10	# WORD - window B start segment
.equ VBEModeInfoBlock.WinFuncPtr,          VBEModeInfoBlock + 12	# LONG - real mode pointer to window function
.equ VBEModeInfoBlock.BytesPerScanLine,    VBEModeInfoBlock + 16	# WORD - bytes per scan line
# Mandatory information for VBE 1.2 and above
.equ VBEModeInfoBlock.XResolution,         VBEModeInfoBlock + 18	# WORD - horizontal resolution in pixels or characters
.equ VBEModeInfoBlock.YResolution,         VBEModeInfoBlock + 20	# WORD - vertical resolution in pixels or characters
.equ VBEModeInfoBlock.XCharSize,           VBEModeInfoBlock + 22	# BYTE - character cell width in pixels
.equ VBEModeInfoBlock.YCharSize,           VBEModeInfoBlock + 23	# BYTE - character cell height in pixels
.equ VBEModeInfoBlock.NumberOfPlanes,      VBEModeInfoBlock + 24	# BYTE - number of memory planes
.equ VBEModeInfoBlock.BitsPerPixel,        VBEModeInfoBlock + 25	# BYTE - bits per pixel
.equ VBEModeInfoBlock.NumberOfBanks,       VBEModeInfoBlock + 26	# BYTE - number of banks
.equ VBEModeInfoBlock.MemoryModel,         VBEModeInfoBlock + 27	# BYTE - memory model type
.equ VBEModeInfoBlock.BankSize,            VBEModeInfoBlock + 28	# BYTE - bank size in KB
.equ VBEModeInfoBlock.NumberOfImagePages,  VBEModeInfoBlock + 29	# BYTE - number of image pages
.equ VBEModeInfoBlock.Reserved,            VBEModeInfoBlock + 30	# BYTE - reserved (0x00 for VBE 1.0-2.0, 0x01 for VBE 3.0)
# Direct Color fields
.equ VBEModeInfoBlock.RedMaskSize,         VBEModeInfoBlock + 31	# BYTE - size of direct color red mask in bits
.equ VBEModeInfoBlock.RedFieldPosition,    VBEModeInfoBlock + 32	# BYTE - bit position of lsb of red mask
.equ VBEModeInfoBlock.GreenMaskSize,       VBEModeInfoBlock + 33	# BYTE - size of direct color green mask in bits
.equ VBEModeInfoBlock.GreenFieldPosition,  VBEModeInfoBlock + 34	# BYTE - bit position of lsb of green mask
.equ VBEModeInfoBlock.BlueMaskSize,        VBEModeInfoBlock + 35	# BYTE - size of direct color blue mask in bits
.equ VBEModeInfoBlock.BlueFieldPosition,   VBEModeInfoBlock + 36	# BYTE - bit position of lsb of blue mask
.equ VBEModeInfoBlock.RsvdMaskSize,        VBEModeInfoBlock + 37	# BYTE - size of direct color reserved mask in bits
.equ VBEModeInfoBlock.RsvdFieldPosition,   VBEModeInfoBlock + 38	# BYTE - bit position of lsb of reserved mask
.equ VBEModeInfoBlock.DirectColorModeInfo, VBEModeInfoBlock + 39	# BYTE - direct color mode attributes
# Mandatory information for VBE 2.0 and above
.equ VBEModeInfoBlock.PhysBasePtr,         VBEModeInfoBlock + 40	# LONG - physical address for flat memory frame buffer
.equ VBEModeInfoBlock.Reserved1,           VBEModeInfoBlock + 44	# LONG - Reserved - always set to 0
.equ VBEModeInfoBlock.Reserved2,           VBEModeInfoBlock + 48	# LONG - Reserved - always set to 0