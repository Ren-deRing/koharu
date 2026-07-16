PHDRS
{
    headers PT_NULL;
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

ENTRY(_stage2)
load = 0x8000;
phys = 0x8000;

SECTIONS
{
	.text load : AT(phys)
	{
		*(.text)
		*(.rodata)
		. = ALIGN(4);
	}
	
	.bss (NOLOAD) :
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