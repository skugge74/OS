#include "vesa.h"
#include "kheap.h"
#include "lib.h"
#include "task.h"

extern struct task task_list[];
static struct multiboot_info* boot_info = 0;
int vesa_cursor_x = 0;
int vesa_cursor_y = 0;
int vesa_dirty = 0;
int vesa_updating = 0; // The LOCK: 1 = Busy drawing, 0 = Safe to flip
extern int current_task_idx;
static uint32_t* back_buffer = NULL;
static uint32_t total_pixels = 0;
static uint32_t screen_width = 0;

uint32_t target_fps = 30; // Global target, default to 30

void VESA_set_fps(uint32_t fps) {
    if (fps == 0) fps = 1;    // Prevent division by zero
    if (fps > 100) fps = 100; // Cap to 100 to avoid CPU melt-down
    target_fps = fps;
}
struct multiboot_info* VESA_get_boot_info() {
    return boot_info;
}
void VESA_print_at(const char* str, int x, int y, uint32_t color) {
    if (!back_buffer) return;
    
    while (*str) {
        VESA_draw_char(*str, x, y, color);
        x += 8; // Advance 8 pixels for the next character
        str++;
    }
    // We don't flip here; we let the timer handle it!
}
void VESA_init(struct multiboot_info* mbi) {
    boot_info = mbi;
    screen_width = boot_info->framebuffer_width;
    total_pixels = screen_width * boot_info->framebuffer_height;
    
    back_buffer = (uint32_t*)kmalloc(total_pixels * 4);
    if (!back_buffer) return;

    VESA_clear();
}

/**
 * Silent clear: RAM only.
 */
void VESA_clear_buffer_only() {
    if (!back_buffer) return;
    vesa_dirty = 1;
    uint32_t bg_color = 0x222222; 

    kmemset(back_buffer, bg_color, total_pixels);
    
    vesa_cursor_x = 0;
    vesa_cursor_y = 0;
}

/**
 * Smart Flip: Only works if dirty AND not locked by vesa_updating.
 */
void VESA_flip() {
    // If vesa_updating is 1, the shell/TOP is still drawing. DO NOT FLIP.
    if (!back_buffer || !vesa_dirty || vesa_updating) return; 

    uint32_t* vram = (uint32_t*)(uintptr_t)boot_info->framebuffer_addr;
    kmemcpy32(vram, back_buffer, total_pixels);

    vesa_dirty = 0; 
}

void VESA_flip_rows(int y, int h) {
    // If updating flag is on, we skip partial flips too to prevent flickering
    if (!back_buffer || vesa_updating) return;

    if (y < 0) y = 0;
    uint32_t* vram = (uint32_t*)(uintptr_t)boot_info->framebuffer_addr;
    uint32_t offset = y * screen_width;
    kmemcpy32(&vram[offset], &back_buffer[offset], h * screen_width);
    
    // We partially synced, but if other parts are still dirty, 
    // we don't clear vesa_dirty here.
}

void VESA_draw_char(char c, int x, int y, uint32_t color) {
    // 1. Safety check: Ensure we have a buffer and aren't drawing off-screen
    if (!back_buffer || x < 0 || y < 0 || 
        x + 8 > (int)screen_width || 
        y + 8 > (int)boot_info->framebuffer_height) return;

    // 2. Metadata tracking (Skip for Shell/Task 0)
    // We use a local pointer to avoid repeated indexing into the volatile array
    if (current_task_idx > 0 && current_task_idx < MAX_TASKS) {
        volatile struct task* cur = &task_list[current_task_idx];
        
        if (!cur->has_drawn) {
            cur->first_x = x;
            cur->first_y = y;
            cur->last_x  = x + 8;
            cur->last_y  = y + 8;
            cur->has_drawn = 1;
        } else {
            // Expand bounding box
            if (x < cur->first_x) cur->first_x = x;
            if (y < cur->first_y) cur->first_y = y;
            if (x + 8 > cur->last_x) cur->last_x = x + 8;
            if (y + 8 > cur->last_y) cur->last_y = y + 8;
        }
    }
    
    // 3. Signal that the frame buffer needs a refresh
    vesa_dirty = 1;

    // 4. Drawing Logic
    uint32_t bg_color = 0x222222;
    uint8_t* glyph = font8x8_basic[(int)c];

    for (int row = 0; row < 8; row++) {
        // Calculate the row start once per row
        uint32_t* dest = &back_buffer[(y + row) * screen_width + x];
        uint8_t data = glyph[row];

        for (int col = 0; col < 8; col++) {
            // Use bitmasking to check font bits
            // (7 - col) handles the bit order of the 8x8 font
            if (data & (1 << (7 - col))) {
                dest[col] = color;
            } else {
                dest[col] = bg_color;
            }
        }
    }
}

void VESA_print(const char* str, uint32_t color) {
    int start_y = vesa_cursor_y;
    while (*str) {
        if (vesa_cursor_y > (int)boot_info->framebuffer_height - 20) {
            VESA_scroll();
            start_y = vesa_cursor_y;
        }

        if (*str == '\n') {
            vesa_cursor_x = 0;
            vesa_cursor_y += 10;
        } else {
            if (vesa_cursor_x + 8 >= (int)screen_width) {
                vesa_cursor_x = 0;
                vesa_cursor_y += 10;
            }
            VESA_draw_char(*str, vesa_cursor_x, vesa_cursor_y, color);
            vesa_cursor_x += 8;
        }
        str++;
    }
    VESA_flip_rows(start_y, 12); 
}

void VESA_print_unsync(const char* str, uint32_t color) {
    while (*str) {
        if (vesa_cursor_y > (int)boot_info->framebuffer_height - 20) {
            VESA_scroll();
        }

        if (*str == '\n') {
            vesa_cursor_x = 0;
            vesa_cursor_y += 10;
        } else {
            if (vesa_cursor_x + 8 >= (int)screen_width) {
                vesa_cursor_x = 0;
                vesa_cursor_y += 10;
            }
            VESA_draw_char(*str, vesa_cursor_x, vesa_cursor_y, color);
            vesa_cursor_x += 8;
        }
        str++;
    }
}

void VESA_scroll() {
    uint32_t line_height = 10;
    vesa_dirty = 1;

    kmemcpy32(back_buffer, &back_buffer[line_height * screen_width], (boot_info->framebuffer_height - line_height) * screen_width);
    kmemset(&back_buffer[(boot_info->framebuffer_height - line_height) * screen_width], 0x222222, line_height * screen_width);
    
    vesa_cursor_y -= line_height;
    
    // Force a full flip on scroll unless we are in the middle of a larger update
    if (!vesa_updating) {
        VESA_flip(); 
    }
}

void VESA_clear() {
    VESA_clear_buffer_only();
    VESA_flip();
}
/**
 * Wipes a specific rectangle in the back buffer.
 * x, y: Top-left corner
 * w, h: Width and Height in pixels
 */
void VESA_clear_region(int x, int y, int w, int h) {
    if (!back_buffer) return;

    // 1. Precise Boundary Checks
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)screen_width) w = screen_width - x;
    if (y + h > (int)boot_info->framebuffer_height) h = boot_info->framebuffer_height - y;
    if (w <= 0 || h <= 0) return;

    uint32_t bg_color = 0x222222; 

    for (int i = 0; i < h; i++) {
        uint32_t* dest = &back_buffer[(y + i) * screen_width + x];
        for (int j = 0; j < w; j++) {
            dest[j] = bg_color; // Correctly sets the full 32-bit pixel
        }
    }

    vesa_dirty = 1; 
}
void kputc(char c) {
    if (!boot_info) return;

    // Handle Newline
    if (c == '\n') {
        vesa_cursor_x = 0;
        vesa_cursor_y += 10; // Match your 10px line height in VESA_print
        return;
    }

    // Handle Carriage Return
    if (c == '\r') {
        vesa_cursor_x = 0;
        return;
    }

    // Wrap-around if we hit the edge of the screen
    if (vesa_cursor_x + 8 >= (int)screen_width) {
        vesa_cursor_x = 0;
        vesa_cursor_y += 10;
    }

    // Check for scrolling
    if (vesa_cursor_y > (int)boot_info->framebuffer_height - 20) {
        VESA_scroll();
    }

    // Draw the actual character
    // Using 0xFFFFFF (White) as default, or pass a global theme color
    VESA_draw_char(c, vesa_cursor_x, vesa_cursor_y, 0xFFFFFF);
    
    // Advance cursor
    vesa_cursor_x += 8;
}
