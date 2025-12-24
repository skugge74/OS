#include "shell.h"
#include "io.h"
#include "lib.h"
#include "idt.h"
#include "pmm.h"
#include "fs.h"
#include "gdt.h"
#include "task.h"
#include "paging.h"
#include "kheap.h"
#include "vesa.h"
// External references for memory and info
extern char end;
extern int system_ticks;

void kmain(uint32_t magic, struct multiboot_info* mbi) {
system_ticks = 0; // Force it to zero here
    (void)magic; 
    if (!(mbi->flags & (1 << 12))) return; 
    //uint32_t* fb = (uint32_t*)(uintptr_t)mbi->framebuffer_addr;
    
    VESA_init(mbi);
    VESA_clear(); // Black background  

    gdt_init(); 
    idt_init();       // Loads LIDT, but DOES NOT call STI yet.
    init_kheap(); 
    uint32_t ram_kb = mbi->mem_upper + 1024;
    pmm_init(ram_kb * 1024, (uint32_t)&end); 
    paging_init(mbi);
    
    init_fs();
    init_multitasking(); 

    // VERY IMPORTANT: Setup the hardware BEFORE enabling interrupts
    pic_remap();      // Re-map IRQs to 32-47
    timer_init(100);  // Start the heartbeat

    VESA_print("KDXOS Kernel Ready. Enabling Interrupts...\n", 0x00FF00);
    kprintf("%d by %d\n", mbi->framebuffer_width, mbi->framebuffer_height);
    // Now, and only now, enable interrupts
    __asm__ volatile("sti");
    
    shell_task();
}
