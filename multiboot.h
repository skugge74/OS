#ifndef MULTIBOOT
#define MULTIBOOT

#include <stdint.h>

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper; // This is the one we want (in KB)
    // ... there are more fields, but we'll start here
} __attribute__((packed));

#endif // !MULTIBOOT
