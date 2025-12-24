#include "paging.h"
#include "vesa.h"
#include <stdint.h>
// A page directory entry
uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t vesa_page_tables[2][1024] __attribute__((aligned(4096)));
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
void paging_init(struct multiboot_info* mbi) {
    // 1. Clear Page Directory
    for(int i = 0; i < 1024; i++) {
        page_directory[i] = 0x00000002; 
    }

    // 2. Identity Map the first 16MB (Kernel, Stack, GDT, IDT, and MBI)
    for (int t = 0; t < 4; t++) {
        for (int i = 0; i < 1024; i++) {
            uint32_t phys = (t * 4 * 1024 * 1024) + (i * 4096);
            kernel_page_tables[t][i] = phys | 3; 
        }
        page_directory[t] = ((uint32_t)kernel_page_tables[t]) | 3;
    }

    // 3. Identity Map the VESA Framebuffer
    uint32_t fb_phys = (uint32_t)mbi->framebuffer_addr;
    uint32_t dir_idx_start = fb_phys >> 22;
    
    for (int t = 0; t < 2; t++) {
        for (int i = 0; i < 1024; i++) {
            // Ensure we are mapping the actual physical pages of the FB
            uint32_t phys = fb_phys + (t * 1024 * 4096) + (i * 4096);
            vesa_page_tables[t][i] = (phys & ~0xFFF) | 3;
        }
        
        // Map the table into the directory
        if ((dir_idx_start + t) < 1024) {
            page_directory[dir_idx_start + t] = ((uint32_t)vesa_page_tables[t]) | 3;
        }
    }

    // 4. Load and Enable
    load_page_directory(page_directory);
    enable_paging();

    // If interrupts were off, turn them back on so the timer works!
    __asm__ volatile("sti"); 

    VESA_print("Paging enabled. Timer should resume...\n", 0x00FF00);
}
