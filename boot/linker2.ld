ENTRY(_stage2)
load = 0x8000;
phys = 0x8000;
SECTIONS
{
	.text load : AT(phys)
	{
		*(.text)
        *(.data)
        *(.rodata)
		. = ALIGN(4);
	}
	.bss : AT(ADDR(.bss))
    {
		. = ALIGN(8);
		__bss_start = .;
        *(COMMON)
        *(.bss)
		. = ALIGN(8);
		__bss_end = .;
    }
	/DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
		*(.note.gnu.build-id)
	}
}