#include "shell.h"
#include "vga.h"
#include "lib.h"
#include "io.h"
#include "idt.h"
#include "pmm.h"
#include "task.h"
#include "fs.h" 

extern uint32_t system_ticks;
extern uint32_t total_pages;
// Helper to find arguments
char* find_space(char* str) {
    while (*str) {
        if (*str == ' ') return str;
        str++;
    }
    return 0;
}
void dummy_app() {
    // 1. Explicitly re-enable interrupts for this task
    __asm__ volatile("sti"); 

    while(1) {
        // 2. Manually yield so the Shell gets a turn
        yield(); 
        
        // 3. Do a tiny bit of work so we see it's alive
        // (This will flicker a character at the top left)
        volatile char* vga = (char*)0xB8000;
        vga[0]++; 
    }
}
void execute_command(char* input) {
    //kprintf("DEBUG: You typed [%s]\n", input);
    char* arg = find_space(input);
    if (arg) {
        *arg = '\0'; // Null-terminate the command part
        arg++;       // Move pointer to the start of the argument
    }

    if (kstrcmp(input, "HELP") == 0) {
        kprintf("Commands: HELP, CLEAR, STATUS, ECHO, CRASH\n");
    } else if (kstrcmp(input, "ECHO") == 0) {
      if (arg) kprintf("%s\n", arg);
      else kprintf("Usage: ECHO <text>\n");
    }

else if (kstrncmp(input, "RUN", 3) == 0) {
        char* filename = input + 4; // Skip "run " to get "clock.bin"
        
        // 1. Get the memory address of the file data
        char* file_data = fs_get_data(filename);
        
        if (file_data != 0) {
            VGA_print("Spawning process: ", COLOR_GREEN);
            VGA_print(filename, COLOR_GREEN);
            VGA_print("\n", COLOR_WHITE);
            
            // 2. Treat that memory address as a function pointer
            spawn_task((void(*)())file_data);
        } else {
            VGA_print("File not found.\n", COLOR_RED);
        }
    }
 
else if (kstrcmp(input, "HEXDUMP") == 0) {
    if (arg) {
        char* data = fs_get_data(arg);
        if (data) {
            // Now we just call the function
            kprintf("Hexdump of %s:\n", arg);
            hexdump(data, 64); 
        } else {
            kprintf("File not found.\n");
        }
    }
}
    else if (kstrcmp(input, "LS") == 0) {
      kprintf("RAMFS Contents:\n");
      for(int i = 0; i < 32; i++) {
        if(fs_is_active(i)) {
            kprintf("- %s \t [%d bytes]\n", fs_get_name(i), fs_get_size(i));
        }
      }
    }
    
    

  else if (kstrcmp(input, "WRITE") == 0) {
        if (arg) {
            char* filename = arg;
            char* content = 0;

            // Search for the space inside the argument string
            for (int i = 0; arg[i] != '\0'; i++) {
                if (arg[i] == ' ') {
                    arg[i] = '\0';        // Terminate filename here
                    content = &arg[i + 1]; // Content starts after the space
                    break;
                }
            }

            if (content) {
                kcreate_file(filename, content);
                kprintf("File '%s' saved with data.\n", filename);
            } else {
                kprintf("Usage: WRITE [filename] [text_with_no_spaces]\n");
            }
        }
    }

    else if (kstrcmp(input, "TOUCH") == 0) {
      if (arg) {
        kcreate_file(arg, "Empty File");
        kprintf("Created file: %s\n", arg);
      }
    }else if (kstrcmp(input, "YIELD") == 0) {
      kprintf("Yielding...\n");
      yield(); 
      // The CPU "pauses" here. 
      // When Task B calls yield, the CPU "wakes up" right here!
      kprintf("Back in Shell!\n");
    }

else if (kstrcmp(input, "LS") == 0) {
    kprintf("RAMFS Contents:\n");
    for(int i = 0; i < 32; i++) {
        if(fs_is_active(i)) {
            kprintf("- %s \t [%d bytes]\n", fs_get_name(i), fs_get_size(i));
        }
    }
}
else if (kstrcmp(input, "CAT") == 0) {
    if (arg) {
        char* data = fs_get_data(arg);
        if (data) {
            kprintf("%s\n", data);
        } else {
            kprintf("File not found: %s\n", arg);
        }
    }
}
  else if (kstrcmp(input, "PS") == 0) {
      kprintf("TID   STATE      ESP\n");
      for (int i = 0; i < 10; i++) {
        if (task_is_ready(i)) {
          kprintf("%d     READY      0x%x\n", i, task_get_esp(i));
        }
      }
    }else if (kstrcmp(input, "KILL") == 0) {
        if (arg) {
            // arg points to the character after the space (e.g., "1")
            int id = arg[0] - '0';
            
            if (id == 0) {
                kprintf("Error: Cannot kill the Shell!\n");
            } else {
                kill_task(id);
                kprintf("Task %d terminated.\n", id);
            }
        } else {
            kprintf("Usage: KILL [id]\n");
        }
    }
  else if (kstrcmp(input, "CLEAR") == 0) {
      VGA_clear();
    }
    /*else if (kstrcmp(input, "GHOST") == 0) {
      kprintf("Writing to 0x200000...\n");
      // volatile ensures the compiler doesn't skip this write
      // unsigned char ensures we handle bytes correctly
      volatile unsigned char* ghost_ptr = (volatile unsigned char*)0x200000;
    
      // Let's write a long string of Red 'X's to make it impossible to miss
      // Row 12, Column 30 is roughly (12 * 80 + 30) * 2 = 1980
      int start = 1980;
      for(int i = 0; i < 20; i+=2) {
        ghost_ptr[start + i] = 'X';      // Character
        ghost_ptr[start + i + 1] = 0x4F; // Bright White on Red background
      }
      kprintf("Done. Look for a Red bar in the center!\n");
    }else if (kstrcmp(input, "MALLOC") == 0) {
      void* ptr1 = pmm_alloc_page();
      void* ptr2 = pmm_alloc_page();
      kprintf("Allocated Page 1 at: %x\n", (uint32_t)ptr1);
      kprintf("Allocated Page 2 at: %x\n", (uint32_t)ptr2);
      kprintf("Difference: %d bytes (Should be 4096)\n", (uint32_t)ptr2 - (uint32_t)ptr1);
    }else if (kstrcmp(input, "MEM") == 0) {
      uint32_t ram_mb = (total_pages * 4096) / 1024 / 1024;
      kprintf("Physical Pages: %d\n", total_pages);
      kprintf("Total RAM:      %d MB\n", ram_mb);
    }else if (kstrcmp(input, "STATUS") == 0) {
        kprintf_color(COLOR_GREEN, "Uptime: %d seconds\n", system_ticks / 1000);
    }*/
    else if (kstrcmp(input, "REBOOT") == 0) {
        kprintf("Rebooting system...\n");  
        // Wait for the keyboard controller to be ready
        uint8_t good = 0x02;
        while (good & 0x02) {
            good = inb(0x64);
        }
        // Send the reset command (0xFE) to the controller
        outb(0x64, 0xFE);
    }else if (kstrcmp(input, "CRASH") == 0) {
        __asm__ volatile("div %0" :: "r"(0)); // Trigger Division Error
    }else if (input[0] == '\0') {
        // Just an empty enter press
    }else {
        kprintf("Unknown command: %s\n", input);
    }
}
