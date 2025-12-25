void pmm_set_page(uint32_t page_addr); 
void pmm_init(uint32_t mem_size, uint32_t bitmap_start); 
int pmm_find_free(); 
void* pmm_alloc_page(); 
