#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include <string.h>

#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"

#include "font.h"
#include "elf.h"

typedef struct {
    uint16_t magic;  // 0x36, 0x04, 0x03, 0x10
    uint8_t  mode;
    uint8_t  char_height;
} __attribute__((packed)) psf1_header_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t pitch; // bytes per scanline
    uint32_t* fb;
} __attribute__((packed)) vbe_screen;

vbe_screen screen;
psf1_header_t font_attribute;

void putc(char c, int x, int y, uint32_t fg) {
    uintptr_t font_addr = (0xFFFF800000000000ULL + (uintptr_t)font);
    uint8_t *glyph = (uint8_t *)font_addr + sizeof(psf1_header_t) + (c * font_attribute.char_height);

    for (int cy = 0; cy < font_attribute.char_height; cy++) {
        uint8_t line = glyph[cy];
        for (int cx = 0; cx < 8; cx++) {
            if (line & (0x80 >> cx)) {
                uint32_t fb_index = (y + cy) * (screen.pitch / 4) + (x + cx);
                screen.fb[fb_index] = fg;
            }
        }
    }
}

int x = 0;
int max_x;
int y = 0;
int max_y;

void dprintf(char const * fmt, ...) {
    char buffer[256]; // i don't care about 256+
    va_list ap; // variable parameters
    va_start(ap, fmt); // get parameters!

    npf_vsnprintf(buffer, sizeof(buffer), fmt, ap); // help me, nanoprintf!

    va_end(ap); // no longer needed.

    // offset = ( y * 80 + x ) * 2, because max_x = 80
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == '\n') { // Newline
            x = 0; // CF
            y++;   // LF
        } else {
            putc(buffer[i], 9*x, (font_attribute.char_height+1)*y, 0xFFFFFFFF); // hard-coded wow
            x++; // VGA text mode basically reads character data from the VGA text buffer and displays.
            
            if (x >= max_x) { // no off-screen
                x = 0;
                y++;
            }
        }

        if (y >= max_y) {
            uintptr_t src_addr = (uintptr_t)screen.fb + (screen.pitch * (font_attribute.char_height + 1));
            int lastline_offset = (screen.pitch * ((max_y - 1) * (font_attribute.char_height + 1)));
            memmove((void *)screen.fb, (const void *)src_addr, lastline_offset); // shift the text data up!
            memset((void*)((uintptr_t)screen.fb + lastline_offset), 0, (size_t)screen.pitch * (font_attribute.char_height + 1)); // clear last line
            y = max_y - 1;
        }
    }
}

typedef struct {
    uint64_t base_addr;     // memory addr
    uint64_t length;        // memory area length
    uint32_t type;          // 1: Good, 2: Reserved, 3: ACPI NVS, 4: Containing bad memory 5: bootloader reclaimable
    uint32_t acpi_ext_attr; // acpi 3.0+ (type 6: bootloader reserved)
    uint64_t padding;
} __attribute__((packed)) mmap_entry_t;

typedef struct {
    uint64_t total_usable;   // usuable memory size
    uint64_t max_phys_addr;  // get_mem_size(true);
    mmap_entry_t* entries;   // E820 list pointer
    uint32_t count;          // entry count
} __attribute__((packed)) boot_mmap_info_t;

uint64_t get_mem_size(bool is_max_addr) {
    volatile mmap_entry_t* mmap_array = (volatile mmap_entry_t*)(uintptr_t)0x8000;
    uint64_t total_size = 0;
    uint64_t max_addr = 0;

    while (mmap_array->base_addr != 0 || mmap_array->length != 0) {
        uint64_t end_addr = mmap_array->base_addr + mmap_array->length;
        
        if (mmap_array->type == 1 || is_max_addr) {
            if (end_addr > max_addr) {
                max_addr = end_addr;
            }
            total_size += mmap_array->length; 
        }
        mmap_array++;
    }
    
    return is_max_addr ? max_addr : total_size; 
}

void add_mmap_entry_split(uint64_t new_base, uint64_t new_len, uint32_t new_type) {
    volatile mmap_entry_t* mmap = (volatile mmap_entry_t*)(uintptr_t)0xFFFF800000008000ULL;
    uint8_t *count_ptr = (uint8_t *)0xFFFF800000006FFFULL;
    uint32_t count = *count_ptr;

    uint64_t new_end = new_base + new_len;

    for (uint32_t i = 0; i < count; i++) {
        uint64_t entry_base = mmap[i].base_addr;
        uint64_t entry_end = entry_base + mmap[i].length;

        // [ENTRY_BASE] <= [NEW_BASE] < [NEW_END] <= [ENTRY_END]
        if (new_base >= entry_base && new_end <= entry_end) {

            bool has_left  = (new_base > entry_base);
            bool has_right = (new_end < entry_end);

            if (has_left && has_right) {
                // cut middle!
                for (uint32_t j = count + 1; j > i + 2; j--) {
                    mmap[j] = mmap[j - 2];
                }

                // Left
                mmap[i].length = new_base - entry_base;

                // New
                mmap[i + 1].base_addr = new_base;
                mmap[i + 1].length = new_len;
                mmap[i + 1].type = new_type;
                mmap[i + 1].acpi_ext_attr = 1;

                // Right
                mmap[i + 2].base_addr = new_end;
                mmap[i + 2].length = entry_end - new_end;
                mmap[i + 2].type = mmap[i].type;
                mmap[i + 2].acpi_ext_attr = 1;

                *count_ptr = count + 2;
            } 
            else if (has_left && !has_right) {
                // cut right entry
                for (uint32_t j = count; j > i + 1; j--) {
                    mmap[j] = mmap[j - 1];
                }

                // Left
                mmap[i].length = new_base - entry_base;

                // New
                mmap[i + 1].base_addr = new_base;
                mmap[i + 1].length = new_len;
                mmap[i + 1].type = new_type;
                mmap[i + 1].acpi_ext_attr = 1;

                *count_ptr = count + 1;
            } 
            else if (!has_left && has_right) {
                // cut left entry
                for (uint32_t j = count; j > i + 1; j--) {
                    mmap[j] = mmap[j - 1];
                }

                // New
                uint32_t old_type = mmap[i].type;
                mmap[i].length = new_len;
                mmap[i].type = new_type;

                // Right
                mmap[i + 1].base_addr = new_end;
                mmap[i + 1].length = entry_end - new_end;
                mmap[i + 1].type = old_type;
                mmap[i + 1].acpi_ext_attr = 1;

                *count_ptr = count + 1;
            } 
            else {
                // NEW = OLD
                mmap[i].type = new_type;
            }

            return;
        }
    }

    // no clips? just append.
    mmap[count].base_addr = new_base;
    mmap[count].length = new_len;
    mmap[count].type = new_type;
    mmap[count].acpi_ext_attr = 1;
    *count_ptr = count + 1;
}

void sort_mmap() {
    volatile mmap_entry_t* mmap = (volatile mmap_entry_t*)(uintptr_t)0xFFFF800000008000ULL;
    uint8_t *count_ptr = (uint8_t *)0xFFFF800000006FFFULL;
    uint32_t count = *count_ptr;

    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = 0; j < count - i - 1; j++) {
            if (mmap[j].base_addr > mmap[j + 1].base_addr) {
                mmap_entry_t temp = mmap[j];
                mmap[j] = mmap[j + 1];
                mmap[j + 1] = temp;
            }
        }
    }
}

void hlt(void) {
    dprintf("\nhalted.");
    for (;;) asm __volatile__ ("hlt");
}

typedef struct {
    uint64_t kernel_src;
    size_t kernel_size;
} __attribute__((packed)) kernel_info_t;

typedef struct {
    vbe_screen screen;
    boot_mmap_info_t memory;

    kernel_info_t kernel;

    uint64_t initrd_addr;
} __attribute__((packed)) boot_info_t;

static uint8_t boot_kernel_stack[16384] __attribute__((aligned(16)));

void jump_main(uint64_t entry, boot_info_t *boot_info) {
    uintptr_t stack_top = (uintptr_t)boot_kernel_stack + sizeof(boot_kernel_stack);
    uintptr_t virt_stack_top = 0xFFFF800000000000ULL + stack_top;

    __asm__ __volatile__(
        "mov %0, %%rsp\n\t"
        "mov %0, %%rbp\n\t"
        "jmp *%1\n\t"
        :
        : "r" (virt_stack_top), "r" (entry), "D" (boot_info)
        : "memory"
    );

    hlt();
}

void loader_entry() {
    uint8_t *vbe_base = (uint8_t *)0x5F00;

    uint64_t vbe_lfb_addr = (uint64_t)*(uint32_t *)(vbe_base + 40);

    screen.fb = (uint32_t *)(0xFFFF800000000000ULL + vbe_lfb_addr);
    screen.width = (uint16_t)*(uint16_t *)(vbe_base + 18);
    screen.height = (uint16_t)*(uint16_t *)(vbe_base + 20);
    screen.pitch = (uint16_t)*(uint16_t *)(vbe_base + 16);

    uint64_t vbe_lfb_end = (uint64_t)vbe_lfb_addr + ((uint64_t)screen.pitch * screen.height);

    extern void init_hhdm(uint64_t mem_size, uint64_t vbe_lfb_end);
    init_hhdm(get_mem_size(true), vbe_lfb_end);

    sort_mmap();

    font_attribute.char_height = *(uint8_t *)((0xFFFF800000000000ULL + (uintptr_t)font) + 3);
    max_x = screen.width / 9; // idk this is clean... i'll fix someday... maybe?
    max_y = screen.height / (font_attribute.char_height + 1);

    dprintf("kinit148\nHello!\n");

    dprintf("mem size: %dMB\n", (get_mem_size(false) >> 20));
    dprintf("vbe lfb addr: 0x%x\n", screen.fb);

    extern int64_t ata_get_bootbin_size();
    int64_t size = ata_get_bootbin_size();
    dprintf("bootbin size: %ld sectors\n", size);
    if (size == -2) {
        dprintf("not a koharu bootbin!\n");
        hlt();
    } else if (size == -1) {
        dprintf("read size failed!\n");
        hlt();
    }

    dprintf("loading bootbin....");
    extern int64_t ata_load_bootbin();
    uint64_t* cpio_base = (uint64_t*)ata_load_bootbin();
    dprintf("ok\n"); // Maybe...?

    extern void* cpio_extract(void *cpio_base_virt, size_t *out_size, char *target_filename);
    size_t kernel_size = 0;
    size_t initrd_size = 0;
    void *kernel_src = cpio_extract(cpio_base, &kernel_size, "kernel.elf");
    void *initrd_src = cpio_extract(cpio_base, &initrd_size, "initrd.cpio");

    if (kernel_src == NULL || initrd_src == NULL) {
        dprintf("failed to load kernel / initrd!\n");
        hlt();
    }

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)kernel_src;

    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        dprintf("Invalid ELF header!\n");
        hlt();
    }

    Elf64_Phdr *phdr = (Elf64_Phdr *)((uint8_t *)kernel_src + ehdr->e_phoff);

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint8_t *dest = (uint8_t *)phdr[i].p_vaddr;
            uint8_t *src = (uint8_t *)kernel_src + phdr[i].p_offset;

            memcpy(dest, src, phdr[i].p_filesz);

            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset(dest + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
            }

            uintptr_t phys_start = phdr[i].p_vaddr - 0xFFFFFFFF80000000ULL;
            uintptr_t page_start = phys_start & ~0xFFFULL;

            uintptr_t phys_end = phys_start + phdr[i].p_memsz;
            uintptr_t page_end = (phys_end + 0xFFFULL) & ~0xFFFULL;
            
            add_mmap_entry_split(page_start, page_end - page_start, 6);
        }
    }

    dprintf("initrd loaded at: 0x%lx\n", initrd_src);

    add_mmap_entry_split((uint64_t)initrd_src - 0xFFFF800000000000ULL, initrd_size, 6);
    add_mmap_entry_split((uint64_t)kernel_src - 0xFFFF800000000000ULL, kernel_size, 5);

    boot_info_t boot_info;
    memset(&boot_info, 0, sizeof(boot_info_t));

    boot_info.screen               = screen;
    boot_info.memory.total_usable  = get_mem_size(false);
    boot_info.memory.max_phys_addr = get_mem_size(true);
    boot_info.memory.entries       = (mmap_entry_t*)0xFFFF800000008000ULL;
    boot_info.memory.count         = *(uint8_t *)0xFFFF800000006FFFULL;
    boot_info.kernel.kernel_src    = (uint64_t)kernel_src;
    boot_info.kernel.kernel_size   = kernel_size;
    boot_info.initrd_addr          = (uint64_t)initrd_src;

    jump_main(ehdr->e_entry,  &boot_info);

    hlt();
}