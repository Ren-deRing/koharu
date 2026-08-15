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
    . = 0x100000;

    . = . + SIZEOF_HEADERS;
    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .text : {
        KEEP(*(entry))
        *(.text .text.*)
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data : {
        *(.data .data.*)
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