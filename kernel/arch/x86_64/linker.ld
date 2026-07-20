ENTRY(generic_entry)

PHDRS
{
    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
}

SECTIONS
{
    . = 0xffffffff80000000;
    
    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .text : {
        __text_start = .;
        *(.text .text.*)
        __text_end = .;
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .pure_initcall : {
        __pure_initcall_start = .;
        QUAD(0);
        KEEP(*(SORT_BY_NAME(.pure_initcall.*)))
        QUAD(0);
        __pure_initcall_end = .;
    } :rodata

    .core_initcall : {
        __core_initcall_start = .;
        QUAD(0);
        KEEP(*(SORT_BY_NAME(.core_initcall.*)))
        QUAD(0);
        __core_initcall_end = .;
    } :rodata

    .arch_initcall : {
        __arch_initcall_start = .;
        QUAD(0);
        KEEP(*(SORT_BY_NAME(.arch_initcall.*)))
        QUAD(0);
        __arch_initcall_end = .;
    } :rodata

    .subsys_initcall : {
        __subsys_initcall_start = .;
        QUAD(0);
        KEEP(*(SORT_BY_NAME(.subsys_initcall.*)))
        QUAD(0);
        __subsys_initcall_end = .;
    } :rodata

    .fs_initcall : {
        __fs_initcall_start = .;
        QUAD(0);
        KEEP(*(SORT_BY_NAME(.fs_initcall.*)))
        QUAD(0);
        __fs_initcall_end = .;
    } :rodata

    .device_initcall : {
        __device_initcall_start = .;
        QUAD(0);
        KEEP(*(SORT_BY_NAME(.device_initcall.*)))
        QUAD(0);
        __device_initcall_end = .;
    } :rodata

    .late_initcall : {
        __late_initcall_start = .;
        QUAD(0);
        KEEP(*(SORT_BY_NAME(.late_initcall.*)))
        QUAD(0);
        __late_initcall_end = .;
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata : {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data : {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    } :data

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } :data

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
    }
}