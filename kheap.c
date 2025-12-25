#include "kheap.h"
#include "lib.h"

// The Linker provides this symbol
extern uint32_t end; 
uint32_t placement_address = (uint32_t)&end;

header_t* heap_start = NULL;
void init_kheap() {
    uint32_t start_node = 0x800000; 

    // CRITICAL: Zero out the memory where the header will live
    // If there is old data here, kmalloc will think the list is corrupted
    kmemset((void*)start_node, 0, sizeof(header_t) / 4); 

    heap_start = (header_t*)start_node;
    heap_start->size = 16 * 1024 * 1024; // 16MB
    heap_start->is_free = 1;
    heap_start->next = NULL;

    // kprintf might fail if VESA isn't up, but let's keep it for serial debug
    // kprintf("Heap safely started at 0x%x\n", start_node);
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
// Use this for Graphics (VESA_flip, VESA_scroll)
void* kmemcpy32(void* dest, const void* src, uint32_t n) {
    __asm__ volatile (
        "rep movsl"
        : "+D"(dest), "+S"(src), "+c"(n)
        : : "memory"
    );
    return dest;
}
void kheap_stats() {
    uint32_t free_mem = 0; 
    uint32_t used_mem = 0;
    uint32_t blocks = 0;
    
    header_t* curr = heap_start;
    uint32_t heap_limit = (uint32_t)heap_start + 0x100000; // 1MB limit

    kprintf("Scanning Heap at 0x%x...\n", (uint32_t)heap_start);

    while (curr != NULL) {
        // --- THE SAFETY CHECK ---
        // If curr is outside the 1MB pool, the list is broken. 
        // Stop here instead of rebooting!
        if ((uint32_t)curr < (uint32_t)heap_start || (uint32_t)curr >= heap_limit) {
            kprintf("Error: Heap linked-list corrupted at 0x%x\n", (uint32_t)curr);
            break;
        }

        if (curr->is_free) {
            free_mem += curr->size;
        } else {
            used_mem += curr->size;
        }
        
        blocks++;

        // Safety: If size is 0, we will loop forever. Stop that.
        if (curr->size == 0 && curr->next != NULL) {
            kprintf("Error: Zero-size block detected.\n");
            break;
        }

        curr = curr->next;
    }

    kprintf("Blocks: %d | Used: %d | Free: %d\n", blocks, used_mem, free_mem);
}

void* kmalloc(uint32_t size) {
    header_t* curr = heap_start;

    while (curr) {
        if (curr->is_free && curr->size >= size) {
            
            // --- THE FIX: Only split if there is REAL room ---
            // You need: Requested Size + Header Size + at least 4 bytes of data
            uint32_t needed_for_split = size + sizeof(header_t) + 4;

            if (curr->size >= needed_for_split) {
                // Calculate position of the new header
                header_t* next_block = (header_t*)((uint32_t)curr + sizeof(header_t) + size);
                
                // Calculate remaining size safely
                next_block->size = curr->size - size - sizeof(header_t);
                next_block->is_free = 1;
                next_block->next = curr->next;

                curr->size = size;
                curr->next = next_block;
            }
            // If it's not big enough to split, we just take the whole block
            // and DON'T subtract the header size from the data size.

            curr->is_free = 0;
            return (void*)((uint32_t)curr + sizeof(header_t));
        }
        curr = curr->next;
    }
    return NULL;
}
void* kmalloc_a(uint32_t size) {
    // 1. We allocate enough extra space to find an aligned spot 
    // without ever moving the actual header.
    uint32_t total_needed = size + 4096; 
    void* ptr = kmalloc(total_needed);
    if (!ptr) return NULL;

    uint32_t addr = (uint32_t)ptr;
    
    // 2. If it's already aligned, just return it
    if (!(addr & 0xFFF)) return ptr;

    // 3. Otherwise, find the next aligned address WITHIN the block we just got
    uint32_t aligned_addr = (addr + 0xFFF) & 0xFFFFF000;
    
    return (void*)aligned_addr;
}
void* kmemset(void* dest, uint32_t val, uint32_t n) {
    __asm__ volatile (
        "rep stosl"
        : "+D"(dest), "+c"(n)
        : "a"(val)
        : "memory"
    );
    return dest;
}
