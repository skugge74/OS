#include "paging.h"
#include "vga.h"
#include <stdint.h>
// A page directory entry
uint32_t page_directory[1024] __attribute__((aligned(4096)));

// We define 4 page tables to cover 16MB of RAM (4MB per table)
uint32_t kernel_page_tables[4][1024] __attribute__((aligned(4096)));

extern void load_page_directory(unsigned int*);
extern void enable_paging();

/**
 * Maps a virtual address to a physical address in the first 4MB
 */
void map_page(uint32_t virtual_addr, uint32_t physical_addr) {
    uint32_t dir_index = virtual_addr >> 22;
    uint32_t table_index = (virtual_addr >> 12) & 0x03FF;

    // For simplicity, we only allow mapping in the first 4MB table via this function
    // unless we dynamically allocated more tables.
    if (dir_index < 4) {
        kernel_page_tables[dir_index][table_index] = (physical_addr & ~0xFFF) | 3;
        flush_tlb();
    }
}

/**
 * Flushes the TLB (Translation Lookaside Buffer) to apply changes immediately
 */
void flush_tlb() {
    uint32_t reg;
    __asm__ volatile("mov %%cr3, %0" : "=r"(reg));
    __asm__ volatile("mov %0, %%cr3" : : "r"(reg));
}

/**
 * Initializes paging by identity mapping the first 16MB of RAM
 */
void paging_init() {
    // 1. Identity map the first 16MB
    // This ensures that physical address X is equal to virtual address X
    for (int t = 0; t < 4; t++) {
        for (int i = 0; i < 1024; i++) {
            uint32_t phys = (t * 4 * 1024 * 1024) + (i * 4096);
            // Attribute 3: Present + Read/Write
            kernel_page_tables[t][i] = phys | 3;
        }
        // Link the page table into the directory
        page_directory[t] = ((uint32_t)kernel_page_tables[t]) | 3;
    }

    // 2. Map Virtual 2MB (0x200000) to Physical 0xB8000 (VGA Buffer) for testing
    // 2MB is index 512 in the first table (t=0)
    kernel_page_tables[0][512] = 0xB8000 | 3;

    // 3. Fill the rest of the directory as "Not Present"
    for (int i = 4; i < 1024; i++) {
        // Attribute 2: Not present, but Read/Write
        page_directory[i] = 0 | 2; 
    }

    // 4. Load the directory into CR3 and set the PG bit in CR0
    load_page_directory(page_directory);
    enable_paging();
    
    VGA_print("Paging Enabled: 16MB Identity Mapped.\n", COLOR_GREEN);
}
