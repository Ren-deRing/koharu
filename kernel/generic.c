void generic_entry(void) {
    for (;;) asm __volatile__ ("hlt");
}