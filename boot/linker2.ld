ENTRY(_enter)
load = 0x8000;
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