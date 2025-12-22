#include <stdint.h>

extern uint32_t end; // Linker symbol marking the end of the kernel
uint32_t placement_address = (uint32_t)&end;

// Basic allocation
void* kmalloc(uint32_t size) {
    uint32_t tmp = placement_address;
    placement_address += size;
    return (void*)tmp;
}

// Aligned allocation (kmalloc_a)
void* kmalloc_a(uint32_t size) {
    // If the current address isn't already aligned to 4KB
    if (placement_address & 0xFFF) {
        // Round up to the next 4KB boundary
        placement_address &= 0xFFFFF000;
        placement_address += 0x1000;
    }
    
    uint32_t tmp = placement_address;
    placement_address += size;
    return (void*)tmp;
}
