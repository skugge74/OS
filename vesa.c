#include "vesa.h"

static struct multiboot_info* boot_info = 0;
int vesa_cursor_x = 0;
int vesa_cursor_y = 0;

void VESA_init(struct multiboot_info* mbi) {
    boot_info = mbi;
    vesa_cursor_x = 0;
    vesa_cursor_y = 0;
}

void VESA_putpixel(int x, int y, uint32_t color) {
    if (x >= (int)boot_info->framebuffer_width || y >= (int)boot_info->framebuffer_height) return;
    
    // Formula: address = base + (y * pitch) + (x * bytes_per_pixel)
    uint32_t* fb = (uint32_t*)(uintptr_t)boot_info->framebuffer_addr;
    uint32_t offset = y * (boot_info->framebuffer_pitch / 4) + x;
    fb[offset] = color;
}

void VESA_clear() {
    uint32_t color = 0x222222;
    uint32_t* fb = (uint32_t*)(uintptr_t)boot_info->framebuffer_addr;
    uint32_t size = (boot_info->framebuffer_pitch / 4) * boot_info->framebuffer_height;
    for (uint32_t i = 0; i < size; i++) {
        fb[i] = color;
    }
    vesa_cursor_x = 0;
    vesa_cursor_y = 0;
}

void VESA_draw_char(char c, int x, int y, uint32_t color) {
    uint32_t bg_color = 0x222222; // Your standard background color

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            // Check the font bit
            if (font8x8_basic[(int)c][row] & (1 << (7 - col))) {
                VESA_putpixel(x + col, y + row, color);    // Draw Foreground
            } else {
                VESA_putpixel(x + col, y + row, bg_color); // Draw Background (ERASE)
            }
        }
    }
}

void VESA_print(const char* str, uint32_t color) {
    for (int i = 0; str[i] != '\0'; i++) {
        
        // --- THE TRIGGER ---
        // Scroll if we are within 2 lines (20px) of the bottom.
        // This prevents the "7 lines too late" issue.
        if (vesa_cursor_y > (int)boot_info->framebuffer_height - 80) {
            VESA_scroll();
        }

        if (str[i] == '\n') {
            vesa_cursor_x = 0;
            vesa_cursor_y += 10;
        } 
        else {
            // Horizontal Wrap
            if (vesa_cursor_x + 8 >= (int)boot_info->framebuffer_width) {
                vesa_cursor_x = 0;
                vesa_cursor_y += 10;
            }

            VESA_draw_char(str[i], vesa_cursor_x, vesa_cursor_y, color);
            vesa_cursor_x += 8;
        }
    }
}
void VESA_display_clock(uint32_t ticks, uint32_t color) {
    uint32_t total_seconds = ticks / 1000;
    uint32_t seconds = total_seconds % 60;
    uint32_t minutes = (total_seconds / 60) % 60;

    char clock_str[6];
    clock_str[0] = (minutes / 10) + '0';
    clock_str[1] = (minutes % 10) + '0';
    clock_str[2] = ':';
    clock_str[3] = (seconds / 10) + '0';
    clock_str[4] = (seconds % 10) + '0';
    clock_str[5] = '\0';

    // Print in the top-right corner
    int x_pos = boot_info->framebuffer_width - 60;
    // Clear the small area for the clock first to avoid overlapping numbers
    for(int i=0; i<10; i++) { // small height
        for(int j=0; j<50; j++) { // small width
             VESA_putpixel(x_pos + j, 10 + i, 0x000000); 
        }
    }
    
    // Draw string at a fixed position
    int temp_x = vesa_cursor_x; // save current cursor
    int temp_y = vesa_cursor_y;
    vesa_cursor_x = x_pos;
    vesa_cursor_y = 10;
    VESA_print(clock_str, color);
    vesa_cursor_x = temp_x; // restore cursor
    vesa_cursor_y = temp_y;
}
void VESA_print_at(const char* str, int x, int y, uint32_t color) {
    while (*str) {
        VESA_draw_char(*str, x, y, color);
        x += 8; // Move to the next character slot
        str++;
    }
}
void VESA_scroll() {
    uint32_t* fb = (uint32_t*)(uintptr_t)boot_info->framebuffer_addr;
    uint32_t pitch = boot_info->framebuffer_pitch / 4; // Pixels per row
    uint32_t line_height = 10;
    uint32_t screen_height = boot_info->framebuffer_height;

    // 1. Move all rows UP by 10 pixels
    // Starting from row 0, we grab pixels from row 10
    for (uint32_t y = 0; y < (screen_height - line_height); y++) {
        // We use kmemcpy if available, otherwise a nested loop is safer:
        for (uint32_t x = 0; x < boot_info->framebuffer_width; x++) {
            fb[y * pitch + x] = fb[(y + line_height) * pitch + x];
        }
    }

    // 2. Clear ONLY the last 10 pixels of the screen
    uint32_t bg_color = 0x222222;
    for (uint32_t y = (screen_height - line_height); y < screen_height; y++) {
        for (uint32_t x = 0; x < boot_info->framebuffer_width; x++) {
            fb[y * pitch + x] = bg_color;
        }
    }

    // 3. IMPORTANT: Reset cursor to the start of the newly cleared line
    vesa_cursor_y = screen_height - line_height;
    vesa_cursor_x = 0; 
}
