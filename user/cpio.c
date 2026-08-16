#include <cpio.h>

#include <string.h>
#include <stdint.h>

typedef struct {
    char magic[6]; // 070701
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];
    char namesize[8];
    char check[8];
} __attribute__((packed)) cpio_header_t;

static inline uint32_t parse_hex8(const char *hex) {
    uint32_t val = 0;
    for (int i = 0; i < 8; i++) {
        val <<= 4;
        if (hex[i] >= '0' && hex[i] <= '9') val |= (hex[i] - '0');
        else if (hex[i] >= 'a' && hex[i] <= 'f') val |= (hex[i] - 'a' + 10);
        else if (hex[i] >= 'A' && hex[i] <= 'F') val |= (hex[i] - 'A' + 10);
    }
    return val;
}

int cpio_extract(void *cpio_base, char *target_filename, uint64_t *out_base,size_t *out_size) {
    uintptr_t base_addr = (uintptr_t)cpio_base;
    uintptr_t current_offset = 0;

    while (1) {
        cpio_header_t *header = (cpio_header_t *)(base_addr + current_offset);

        // check magic number (070701)
        if (strncmp(header->magic, "070701", 6) != 0) {
            return -1; // how?
        }

        uint32_t filesize = parse_hex8(header->filesize);
        uint32_t namesize = parse_hex8(header->namesize);

        char *filename = (char *)(base_addr + current_offset + sizeof(cpio_header_t));

        // TRAILER!!!
        if (strncmp(filename, "TRAILER!!!", 10) == 0) {
            break;
        } // i like that

        // find file data start
        // hader + namesize + padding (4B)
        size_t data_offset = sizeof(cpio_header_t) + namesize;
        data_offset = (data_offset + 3) & ~3; // 4B padding


        if (strcmp(filename, target_filename) == 0) { // is this....?
            *out_base = (base_addr + current_offset + data_offset);
            *out_size = filesize;
            return 0; // oh good let's go
        }

        // next file
        size_t next_record_offset = data_offset + filesize;
        next_record_offset = (next_record_offset + 3) & ~3;

        current_offset += next_record_offset;
    }

    return -1; // no target_filename here....
}