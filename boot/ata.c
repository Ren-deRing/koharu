#include <stdint.h>

#define ATA_REG_DATA        0x1F0
#define ATA_REG_FEATURES    0x1F1
#define ATA_REG_SECCOUNT    0x1F2
#define ATA_REG_LBA_LO      0x1F3
#define ATA_REG_LBA_MID     0x1F4
#define ATA_REG_LBA_HI      0x1F5
#define ATA_REG_DRIVE       0x1F6
#define ATA_REG_STATUS      0x1F7
#define ATA_REG_COMMAND     0x1F7

static inline void outb(uint16_t port, uint8_t val) {
    asm __volatile__ ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm __volatile__ ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm __volatile__ ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static uint8_t selected_drive = 0xFF;

void ata_select_drive(uint8_t drive) {
    if (drive == selected_drive) {
        return;
    }
    
    outb(ATA_REG_DRIVE, drive);
    selected_drive = drive;
    
    for(int i = 0; i < 15; i++) { // come on......
        inb(ATA_REG_STATUS);
    } // any day now...
}

void ata_read_sectors(uint64_t lba_addr, uint64_t sectors, uint16_t* buffer) {
    ata_select_drive(0xE0 | ((lba_addr >> 24) & 0x0F)); // LBA [27:24]

    outb(ATA_REG_SECCOUNT, (uint8_t)sectors); // how many sectors to read
    outb(ATA_REG_LBA_LO, (uint8_t)(lba_addr & 0xFF)); // LBA [7:0]
    outb(ATA_REG_LBA_MID, (uint8_t)((lba_addr >> 8) & 0xFF)); // LBA [15:8]
    outb(ATA_REG_LBA_HI, (uint8_t)((lba_addr >> 16) & 0xFF)); // LBA [23:16]

    outb(ATA_REG_COMMAND, 0x20); // READ!

    for (int i = 0; i < 4; i++) { // bus is slow....
        inb(ATA_REG_STATUS);
    }

    for (uint64_t s = 0; s < sectors; s++) {
        // wait until (!BSY(0x80)(BUSY) && DRQ(0x08)(DATA READY))
        while (1) {
            uint8_t status = inb(ATA_REG_STATUS);
            if ((status & 0x80) == 0 && (status & 0x08) != 0) {
                break;
            }
            if (status & 0x01) {
                return;
            }
        }

        for (int w = 0; w < 256; w++) {
            *buffer = inw(ATA_REG_DATA); // read
            buffer++;
        }
    }
}

int64_t ata_get_bootbin_size() {
    ata_select_drive(0xE0);
    uint32_t* header_buffer = (uint32_t*)(0xFFFF800000000000ULL + 0x30000); // anyway...
    ata_read_sectors(40, 1, (uint16_t*)header_buffer);
    uint32_t bootbin_sectors = header_buffer[1];
    
    if (bootbin_sectors > 0) {
        if (header_buffer[0] != 0xC0FA1480) {
            return -2;
        }
        return bootbin_sectors;
    }

    return -1;
}

uint32_t* ata_load_bootbin() {
    // kernel is located at the 41th sector (0x28)
    ata_select_drive(0xE0); // 0xE0 => 1(FIXED)1(LBA)1(FIXED)0(MASTER)0000(LBA [27:24])
    uint32_t* bootbin_buffer = (uint32_t*)(0xFFFF800000000000ULL + 0x400000); // anyway... (v2)
    ata_read_sectors(41, ata_get_bootbin_size(), (uint16_t*)bootbin_buffer);
    return bootbin_buffer;
}