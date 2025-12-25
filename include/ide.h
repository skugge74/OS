#ifndef IDE_H
#define IDE_H
#include <stdint.h>
#define IDE_PRIMARY_DATA       0x1F0
#define IDE_PRIMARY_ERR        0x1F1
#define IDE_PRIMARY_SECCOUNT   0x1F2
#define IDE_PRIMARY_LBA_LOW    0x1F3
#define IDE_PRIMARY_LBA_MID    0x1F4
#define IDE_PRIMARY_LBA_HIGH   0x1F5
#define IDE_PRIMARY_DRIVE_SEL  0x1F6
#define IDE_PRIMARY_COMMAND    0x1F7

void ide_read_sector(uint32_t lba, uint8_t* buffer); 
#endif
