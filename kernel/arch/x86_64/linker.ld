ENTRY(generic_entry)

PHDRS
{
    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
}

SECTIONS
{
    . = 0xFFFFFFFF82000000;
    
    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .text : {
        __text_start = .;
        *(.text.entry)
        *(.text .text.*)
        __text_end = .;
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .early_initcall : {
        __early_initcall_start = .;
        KEEP(*(SORT_BY_NAME(.early_initcall.*)))
        __early_initcall_end = .;
    } :rodata

    .core_initcall : {
        __core_initcall_start = .;
        KEEP(*(SORT_BY_NAME(.core_initcall.*)))
        __core_initcall_end = .;
    } :rodata

    .arch_initcall : {
        __arch_initcall_start = .;
        KEEP(*(SORT_BY_NAME(.arch_initcall.*)))
        __arch_initcall_end = .;
    } :rodata

    .sys_initcall : {
        __sys_initcall_start = .;
        KEEP(*(SORT_BY_NAME(.sys_initcall.*)))
        __sys_initcall_end = .;
    } :rodata

    .late_initcall : {
        __late_initcall_start = .;
        KEEP(*(SORT_BY_NAME(.late_initcall.*)))
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