
#include <stdint.h>

void paging_init(); 
void map_page(uint32_t virtual_addr, uint32_t physical_addr); 
void flush_tlb(); 
