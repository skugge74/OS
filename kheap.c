#include "kheap.h"
#include "lib.h"

// The Linker provides this symbol
extern uint32_t end; 
uint32_t placement_address = (uint32_t)&end;

header_t* heap_start = NULL;

void init_kheap() {
    // Align the start to 4KB for safety
    if (placement_address & 0xFFF) {
        placement_address &= 0xFFFFF000;
        placement_address += 0x1000;
    }

    heap_start = (header_t*)placement_address;
    heap_start->size = 0x100000; // Let's start with a 1MB initial "pool"
    heap_start->is_free = 1;
    heap_start->next = NULL;

    placement_address += (sizeof(header_t) + heap_start->size);
}

void* kmalloc(uint32_t size) {
    header_t* curr = heap_start;

    while (curr) {
        // Find a block that is FREE and BIG ENOUGH
        if (curr->is_free && curr->size >= size) {
            
            // Can we split? (Need room for: requested size + new header + small buffer)
            if (curr->size > (size + sizeof(header_t) + 32)) {
                // 1. Create the new header for the remaining space
                header_t* next_block = (header_t*)((uint32_t)curr + sizeof(header_t) + size);
                
                // 2. Set metadata for the new free block
                next_block->size = curr->size - size - sizeof(header_t);
                next_block->is_free = 1;
                next_block->next = curr->next;

                // 3. Shrink current block to requested size and link it to the new one
                curr->size = size;
                curr->next = next_block;
            }

            // 4. MARK AS USED - This is what makes STAT work!
            curr->is_free = 0;

            // Return the data area (after the header)
            return (void*)((uint32_t)curr + sizeof(header_t));
        }
        curr = curr->next;
    }
    return NULL; // No memory left!
}

void kfree(void* ptr) {
    if (!ptr) return;

    // 1. Find the header
    header_t* header = (header_t*)((uint32_t)ptr - sizeof(header_t));
    header->is_free = 1;

    // 2. Coalesce (Merge with next block if it is also free)
    if (header->next && header->next->is_free) {
        // Add the size of the next block + its header to this block
        header->size += sizeof(header_t) + header->next->size;
        // Skip the next block in the list
        header->next = header->next->next;
    }

    // Note: To be perfect, we should also merge with the PREVIOUS block, 
    // but that requires a "doubly linked list." For now, this helps a lot!
}

void* kmalloc_a(uint32_t size) {
    // 1. Calculate how much extra space we need to guarantee alignment
    // We add 4KB to ensure we can find a spot that starts at 0x...000
    uint32_t total_needed = size + 0x1000; 
    
    // 2. Call the REAL kmalloc that handles headers and stats
    void* ptr = kmalloc(total_needed);
    
    // 3. Align the returned pointer
    uint32_t addr = (uint32_t)ptr;
    if (addr & 0xFFF) {
        addr &= 0xFFFFF000;
        addr += 0x1000;
    }
    
    return (void*)addr;
}

void* kmemcpy(void* dest, const void* src, uint32_t n) {
    __asm__ volatile (
        "movl %0, %%edi\n"
        "movl %1, %%esi\n"
        "movl %2, %%ecx\n"
        "rep movsb"
        : 
        : "r"(dest), "r"(src), "r"(n)
        : "edi", "esi", "ecx", "memory"
    );
    return dest;
}

void kheap_stats() {
    uint32_t free_mem = 0; 
    uint32_t used_mem = 0;
    uint32_t blocks = 0;
    
    // START walking from the existing heap_start
    header_t* curr = heap_start;

    while (curr) {
        if (curr->is_free) {
            free_mem += curr->size;
        } else {
            used_mem += curr->size;
        }
        blocks++;
        curr = curr->next;
    }

    kprintf("Heap Status: %d blocks\n", blocks);
    kprintf("Used: %d bytes | Free: %d bytes\n", used_mem, free_mem);
}
