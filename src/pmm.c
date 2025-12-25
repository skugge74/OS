#include <stdint.h>
#include "pmm.h"
uint32_t* bitmap;
uint32_t total_pages;

// Define this first!
void pmm_set_page(uint32_t page_addr) {
    uint32_t frame = page_addr / 4096;
    uint32_t idx = frame / 32;
    uint32_t off = frame % 32;
    bitmap[idx] |= (1 << off);
}

void pmm_init(uint32_t mem_size, uint32_t bitmap_start) {
    total_pages = mem_size / 4096;
    bitmap = (uint32_t*)bitmap_start;

    // Clear bitmap (free all memory)
    for (uint32_t i = 0; i < total_pages / 32; i++) {
        bitmap[i] = 0;
    }
    
    // Mark the kernel and the bitmap itself as used
    // We'll mark the first 4MB as used just to be safe for now
    for (uint32_t i = 0; i < 1024; i++) {
        pmm_set_page(i * 4096);
    }
}
// Returns the index of the first free bit (0) found in the bitmap
int pmm_find_free() {
    for (uint32_t i = 0; i < total_pages / 32; i++) {
        if (bitmap[i] != 0xFFFFFFFF) { // If this uint32 is not full
            for (int j = 0; j < 32; j++) {
                uint32_t bit = 1 << j;
                if (!(bitmap[i] & bit)) {
                    return i * 32 + j;
                }
            }
        }
    }
    return -1; // Out of memory!
}

void* pmm_alloc_page() {
    int frame = pmm_find_free();
    if (frame == -1) return 0;

    pmm_set_page(frame * 4096);
    return (void*)(frame * 4096);
}
