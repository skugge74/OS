#include "vga.h"
#include <stdarg.h>
#include "lib.h"
#include <stddef.h>

int kstrcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return *(unsigned char*)a - *(unsigned char*)b;
}

int katoi(char* str) {
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i) {
        if (str[i] < '0' || str[i] > '9') break;
        res = res * 10 + str[i] - '0';
    }
    return res;
}

char* kstrcpy(char* dest, const char* src) {
    char* saved = dest;
    while ((*dest++ = *src++) != '\0');
    return saved;
}

int kstrncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        // If characters are different, or we hit the end of s1
        if (s1[i] != s2[i] || s1[i] == '\0') {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
    }
    // If we checked n characters and they were all the same
    return 0;
}

char* kstrncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for ( ; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

size_t kstrlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;
            switch (format[i]) {
                case 'c': {
                    char c = (char)va_arg(args, int);
                    char str[2] = {c, '\0'};
                    VGA_print(str, 0x07);
                    break;
                }
                case 's': {
                    char* s = va_arg(args, char*);
                    VGA_print(s, 0x07);
                    break;
                }
                case 'd': {
                    int d = va_arg(args, int);
                    char buf[32];
                    itoa(d, buf, 10);
                    VGA_print(buf, 0x07);
                    break;
                }
                case 'x': {
                    int x = va_arg(args, int);
                    char buf[32];
                    itoa(x, buf, 16);
                    //VGA_print("0x", 0x07); // Prefix for hex
                    VGA_print(buf, 0x07);
                    break;
                }
                default:
                    VGA_print("%", 0x07);
                    break;
            }
        } else {
            char str[2] = {format[i], '\0'};
            VGA_print(str, 0x07);
        }
    }
    va_end(args);
}

void kprintf_color(int color, const char* format, ...) {
    va_list args;
    va_start(args, format);

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;
            switch (format[i]) {
                case 's': {
                    char* s = va_arg(args, char*);
                    VGA_print(s, color); // Use the passed color!
                    break;
                }
                case 'd': {
                    int d = va_arg(args, int);
                    char buf[32];
                    itoa(d, buf, 10);
                    VGA_print(buf, color);
                    break;
                }
                case 'x': {
                    int x = va_arg(args, int);
                    char buf[32];
                    itoa(x, buf, 16);
                    VGA_print("0x", color);
                    VGA_print(buf, color);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    char str[2] = {c, '\0'};
                    VGA_print(str, color);
                    break;
                }
            }
        } else {
            char str[2] = {format[i], '\0'};
            VGA_print(str, color);
        }
    }
    va_end(args);
}

// Integer to ASCII
void itoa(int n, char* str, int base) {
    int i = 0;
    int isNegative = 0;

    if (n == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    if (n < 0 && base == 10) {
        isNegative = 1;
        n = -n;
    }

    while (n != 0) {
        int rem = n % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
        n = n / base;
    }

    if (isNegative) str[i++] = '-';

    str[i] = '\0';

    // The digits are currently backward, let's reverse them
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}
extern uint32_t system_ticks;

void hexdump(void* ptr, int size) {
    unsigned char* data = (unsigned char*)ptr;
    
    for (int i = 0; i < size; i++) {
        // 1. Hex Part
        unsigned char b = data[i];
        kprintf("0x");
        if (b < 0x10) kprintf("0");
        kprintf("%x ", b);

        // 2. ASCII Sidebar logic (Every 8 bytes)
        if ((i + 1) % 8 == 0) {
            kprintf(" | ");
            for (int j = i - 7; j <= i; j++) {
                unsigned char c = data[j];
                // Only print printable ASCII (32 is space, 126 is ~)
                if (c >= 32 && c <= 126) {
                    // Assuming your kprintf supports %c
                    kprintf("%c", c);
                } else {
                    kprintf("."); // Non-printable
                }
            }
            kprintf("\n");
        }
    }
    // Handle the case where size isn't a multiple of 8
    if (size % 8 != 0) {
        kprintf("\n");
    }
}
void sleep(int ms) {
    __asm__ volatile (
        "mov $3, %%eax \n" // Syscall number 3
        "mov %0, %%ebx \n" // Number of ms
        "int $0x80     \n"
        : : "r"(ms) : "eax", "ebx"
    );
}
