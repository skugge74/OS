#include "vga.h"
#include "io.h"

// Actual definitions
char *vidptr = (char*)0xb8000;
int cursor_pos = 0;

void update_cursor(int pos) {
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}
void VGA_print_at(const char* str, int color, int x, int y) {
    // Calculate the offset: (Row * 80 + Column) * 2 bytes per char
    int offset = (y * 80 + x) * 2;
    
    for(int i = 0; str[i] != '\0'; i++) {
        vidptr[offset] = str[i];
        vidptr[offset + 1] = color;
        offset += 2;
        
        // Safety: don't wrap or overflow the buffer in this simple version
        if (offset >= 80 * 25 * 2) break;
    }
}
void VGA_display_clock(uint32_t ticks) {
    // 1000 ticks = 1 second (if timer_init was 1000)
    uint32_t total_seconds = ticks / 1000;
    uint32_t seconds = total_seconds % 60;
    uint32_t minutes = (total_seconds / 60) % 60;

    char clock_str[10];
    // Simple formatting: "MM:SS"
    clock_str[0] = (minutes / 10) + '0';
    clock_str[1] = (minutes % 10) + '0';
    clock_str[2] = ':';
    clock_str[3] = (seconds / 10) + '0';
    clock_str[4] = (seconds % 10) + '0';
    clock_str[5] = '\0';

    // VGA Buffer is at 0xB8000. 
    // Top right corner (Row 0, Col 70) = (0 * 80 + 70) * 2 = 140
    uint16_t* terminal_buffer = (uint16_t*) 0xB8000;
    for (int i = 0; clock_str[i] != '\0'; i++) {
        // 0x1F = White text on Blue background
        terminal_buffer[74 + i] = (uint16_t)clock_str[i] | (uint16_t)0x1F << 8;
    }
}
void VGA_clear() {
    for(int j=0; j < 80*25*2; j+=2) {
        vidptr[j] = ' ';
        vidptr[j+1] = 0x07; // Light grey on black
    }
    cursor_pos = 0;
    update_cursor(0);
}

void scroll() {
    // Move rows 1-24 up to 0-23
    for (int i = 0; i < 24 * 80; i++) {
        vidptr[i * 2] = vidptr[(i + 80) * 2];
        vidptr[i * 2 + 1] = vidptr[(i + 80) * 2 + 1];
    }
    // Clear the last row
    for (int i = 24 * 80; i < 25 * 80; i++) {
        vidptr[i * 2] = ' ';
        vidptr[i * 2 + 1] = 0x07;
    }
    cursor_pos = 24 * 80;
}

void VGA_print(const char* str, int color) {
    for(int i=0; str[i] != '\0'; i++) {
        if (cursor_pos >= 2000) scroll();

        if(str[i] == '\n') {
            cursor_pos = ((cursor_pos / 80) + 1) * 80;
        } 
        else if(str[i] == '\b') {
            if (cursor_pos > 0) {
                cursor_pos--;
                vidptr[cursor_pos * 2] = ' ';
                vidptr[cursor_pos * 2 + 1] = 0x07;
            }
        } 
        else {
            vidptr[cursor_pos * 2] = str[i];
            vidptr[cursor_pos * 2 + 1] = color;
            cursor_pos++;
        }
    }
    update_cursor(cursor_pos);
}
