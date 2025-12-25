#include "io.h"
#include <stdint.h>
#include "ide.h"
void ide_read_sector(uint32_t lba, uint8_t* buffer) {
    outb(IDE_PRIMARY_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(IDE_PRIMARY_SECCOUNT, 1);
    outb(IDE_PRIMARY_LBA_LOW, (uint8_t)lba);
    outb(IDE_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(IDE_PRIMARY_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(IDE_PRIMARY_COMMAND, 0x20); // 0x20 is "Read Sectors"

    // Wait for the drive to be ready
    while (!(inb(IDE_PRIMARY_COMMAND) & 0x08));

    // Read 256 16-bit words (512 bytes total)
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(IDE_PRIMARY_DATA);
    }
}
