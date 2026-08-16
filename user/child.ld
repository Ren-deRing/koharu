ENTRY(_start)

PHDRS
{
    text   PT_LOAD FILEHDR PHDRS;
    rodata PT_LOAD;
    data   PT_LOAD;
    tls    PT_TLS;
}

SECTIONS
{
    . = 0x400000;

    __ehdr_start = .;

    . = . + SIZEOF_HEADERS;
    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .text : {
        KEEP(*(.text*))
        *(.text .text.*)
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    __preinit_array_start = .;
    .preinit_array : { *(.preinit_array*) }
    __preinit_array_end = .;

    __init_array_start = .;
    .init_array : { *(.init_array*) }
    __init_array_end = .;

    __fini_array_start = .;
    .fini_array : { *(.fini_array*) }
    __fini_array_end = .;

    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
    } :data

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    . = ALIGN(64);

    .tdata : {
        *(.tdata .tdata.*)
    } :tls :data

    .tbss : {
        *(.tbss .tbss.*)
    } :tls :data

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
    }
}