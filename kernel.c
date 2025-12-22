#include "vga.h"
#include "shell.h"
#include "io.h"
#include "lib.h"
#include "idt.h"
#include "pmm.h"
#include "fs.h"
#include "gdt.h"
#include "multiboot.h"
#include "task.h"
#include "paging.h"

// External references for memory and info
struct multiboot_info* mb_info;
extern char end;


void kmain(uint32_t magic, uint32_t addr) {
    mb_info = (struct multiboot_info*)addr;
    VGA_clear();
    
    gdt_init();
    idt_init();       // Loads LIDT, but DOES NOT call STI yet.
    
    uint32_t ram_kb = mb_info->mem_upper + 1024;
    pmm_init(ram_kb * 1024, (uint32_t)&end); 
    paging_init();
    
    init_fs();
    init_multitasking(); 

    // VERY IMPORTANT: Setup the hardware BEFORE enabling interrupts
    pic_remap();      // Re-map IRQs to 32-47
    timer_init(100);  // Start the heartbeat

    VGA_print("KDXOS Kernel Ready. Enabling Interrupts...\n", COLOR_CYAN);

    // Now, and only now, enable interrupts
    __asm__ volatile("sti");

    shell_task();
}
