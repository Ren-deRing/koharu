ENTRY(_start)
load = 0x7c00;
phys = 0x0;
SECTIONS
{
	.text load : AT(phys)
	{
		*(.text)
        *(.data)
        *(.rodata)
		. = ALIGN(4);
	}
	/DISCARD/ :
	{
		*(.bss)
		*(.comment)
		*(.eh_frame)
		*(.note.gnu.build-id)
	}
}