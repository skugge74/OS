#ifndef VGA_H
#define VGA_H

#include <stdint.h>

#define COLOR_WHITE  0x0F
#define COLOR_GREEN  0x0A
#define COLOR_RED    0x0C
#define COLOR_YELLOW 0x0E
#define COLOR_CYAN   0x0B

// Usage helpers
#define kinfo(fmt, ...)  kprintf_color(COLOR_CYAN,   "[INFO] " fmt "\n", ##__VA_ARGS__)
#define kwarn(fmt, ...)  kprintf_color(COLOR_YELLOW, "[WARN] " fmt "\n", ##__VA_ARGS__)
#define kerror(fmt, ...) kprintf_color(COLOR_RED,    "[ERR ] " fmt "\n", ##__VA_ARGS__)

// Extern tells the compiler these are defined in vga.c
extern char *vidptr;
extern int cursor_pos;

/* --- VGA Driver Prototypes --- */
void update_cursor(int pos);
void VGA_clear();
void scroll();
void VGA_print(const char* str, int color);
void VGA_display_clock(uint32_t ticks);
void VGA_print_at(const char* str, int color, int x, int y);
#endif
