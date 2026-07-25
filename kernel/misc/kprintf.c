typedef void log_putc(char c);

log_putc kprintf_log;

// void kputc(char c) {
//     if (kprintf_log) {
//         kprintf_log(c);
//     }
// }