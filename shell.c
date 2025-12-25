#include "shell.h"
#include "vesa.h"
#include "lib.h"
#include "io.h"
#include "idt.h"
#include "pmm.h"
#include "task.h"
#include "fs.h" 
#include "kheap.h" 
#include "idt.h"

extern int vesa_updating;
extern uint32_t system_ticks;
extern uint32_t total_pages;
extern uint32_t timer_frequency;        // Add this!
extern uint32_t target_fps;
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
    char* arg = find_space(input);
    if (arg) {
        *arg = '\0'; 
        arg++;       
    }
    int start_y = vesa_cursor_y;
    vesa_updating = 1;
    if (kstrcmp(input, "HELP") == 0) {
        kprintf_unsync("Commands: HELP, ECHO, RUN, REBOOT, CRASH, STAT, LS, PS, TOP, CLEAR\n");
    }
else if (kstrcmp(input, "SET_FPS") == 0) {
    if (arg) {
        target_fps = katoi(arg);
        kprintf_unsync("FPS set to %d\n", target_fps);
    }
}
    else if (kstrcmp(input, "TIMER") == 0){
    // We pass 0 for 'raw_code' because it's a kernel-space function, not an loaded file.
    int tid = spawn_task(task_timer, 0, "timer");
    kprintf_unsync("Timer task spawned (TID: %d)\n", tid);
    }
    else if (kstrcmp(input, "ECHO") == 0) {
        if (arg) kprintf_unsync("%s\n", arg);
        else kprintf_unsync("Usage: ECHO [text]\n");
    }
    else if (kstrcmp(input, "STAT") == 0) {
        // Assuming kheap_stats now uses unsync internal prints
        kheap_stats();  
    }
    else if (kstrcmp(input, "SLEEP") == 0) {
        if (arg) {
            int ms = katoi(arg);
            if (ms > 0) {
                kprintf_unsync("Shell sleeping for %d ms...\n", ms);
                VESA_flip(); // Flip here because we are about to block/sleep
                sleep(ms); 
                kprintf_unsync("Shell awake.\n");
            }
        } else {
            kprintf_unsync("Usage: SLEEP [ms]\n");
        }
    }
    else if (kstrcmp(input, "RUN") == 0) {
        if (arg) {
            int idx = -1;
            for (int i = 0; i < MAX_FILES; i++) {
                if (fs_is_active(i) && kstrcmp(fs_get_name(i), arg) == 0) {
                    idx = i;
                    break;
                }
            }
            if (idx != -1) {
                char* file_data = fs_get_data(arg);
                uint32_t size = fs_get_size(idx);
                void* raw_code = kmalloc(size + 4096);
                if (!raw_code) {
                    kprintf_unsync("Memory allocation failed\n");
                } else {
                    uint32_t aligned_code = ((uint32_t)raw_code + 0xFFF) & 0xFFFFF000;
                    kmemcpy((void*)aligned_code, file_data, size);
                    int tid = spawn_task((void(*)())aligned_code, raw_code, arg);
                    kprintf_unsync("Spawned %s (TID: %d, Entry: 0x%x)\n", arg, tid, aligned_code);
                }
            } else {
                kprintf_unsync("File not found: %s\n", arg);
            }
        }
    }
    else if (kstrcmp(input, "TEST_MALLOC") == 0) {
        kprintf_unsync("Allocating 100KB...\n");
        void* p = kmalloc(102400); 
        kheap_stats();
        kprintf_unsync("Freeing 100KB...\n");
        kfree(p);
        kheap_stats();
    }
    else if (kstrcmp(input, "HEXDUMP") == 0) {
        if (arg) {
            int found_idx = -1;
            for (int i = 0; i < MAX_FILES; i++) {
                if (fs_is_active(i) && kstrcmp(fs_get_name(i), arg) == 0) {
                    found_idx = i;
                    break;
                }
            }
            if (found_idx != -1) {
                char* data = fs_get_data(arg);
                uint32_t size = fs_get_size(found_idx);
                kprintf_unsync("Dumping %s (%d bytes):\n", arg, size);
                // Ensure your hexdump function uses kprintf_unsync internally!
                hexdump((void*)data, size); 
            } else {
                kprintf_unsync("File not found: %s\n", arg);
            }
        }
    }
    else if (kstrcmp(input, "LS") == 0) {
        kprintf_unsync("RAMFS Contents:\n");
        for(int i = 0; i < 32; i++) {
            if(fs_is_active(i)) {
                kprintf_unsync("- %s \t [%d bytes]\n", fs_get_name(i), fs_get_size(i));
            }
        }
    }
    else if (kstrcmp(input, "WRITE") == 0) {
        if (arg) {
            char* filename = arg;
            char* content = 0;
            for (int i = 0; arg[i] != '\0'; i++) {
                if (arg[i] == ' ') {
                    arg[i] = '\0';
                    content = &arg[i + 1];
                    break;
                }
            }
            if (content) {
                kcreate_file(filename, content);
                kprintf_unsync("File '%s' saved.\n", filename);
            } else {
                kprintf_unsync("Usage: WRITE [filename] [text]\n");
            }
        }
    }
    else if (kstrcmp(input, "PS") == 0) {
        kprintf_unsync("TID   NAME         STATE\n");
        for (int i = 0; i < MAX_TASKS; i++) {
            if (task_is_ready(i)) {
                char* name = task_get_name(i);
                kprintf_unsync("%d     %s", i, name);
                int len = kstrlen(name);
                for (int j = 0; j < (12 - len); j++) kprintf_unsync(" ");
                
                if (task_get_state(i) == 1)      kprintf_unsync(" READY\n");
                else if (task_get_state(i) == 2) kprintf_unsync(" SLEEP\n");
            }
        }
    }
    else if (kstrcmp(input, "TOP") == 0) {
        run_top(); 
        VESA_clear(); // Clear back to shell after exiting TOP
    }
    else if (kstrcmp(input, "UPTIME") == 0) {
        uint32_t current_ticks = system_ticks;
        uint32_t current_freq = 100; // Assuming 100Hz
        uint32_t s = current_ticks / current_freq;
        kprintf_unsync("Uptime: %ds (Ticks: %d)\n", s, current_ticks);
    }
    else if (kstrcmp(input, "KILL") == 0) {
        if (arg) {
            int id = arg[0] - '0';
            if (id == 0) kprintf_unsync("Error: Cannot kill Shell\n");
            else {
                kill_task(id);
                kprintf_unsync("Task %d killed.\n", id);
            }
        }
    }
    else if (kstrcmp(input, "CLEAR") == 0) {
        VESA_clear_buffer_only(); 
        // No need to flip here, the final flip handles it
    }
    else if (kstrcmp(input, "REBOOT") == 0) {
        kprintf_unsync("Rebooting...\n");
        VESA_flip(); // Must flip so user sees message before CPU resets
        outb(0x64, 0xFE);
    }
    else if (kstrcmp(input, "CRASH") == 0) {
        kprintf_unsync("Triggering CPU Exception...\n");
        VESA_flip();
        __asm__ volatile("div %0" :: "r"(0));
    }
    else if (input[0] != '\0') {
        kprintf_unsync("Unknown command: %s\n", input);
    }
   
    int lines_touched = (vesa_cursor_y - start_y) + 12;
    vesa_updating = 0; 
    struct multiboot_info* boot_info = VESA_get_boot_info();
    if (lines_touched > 0 && lines_touched < (int)boot_info->framebuffer_height) {
        VESA_flip_rows(start_y, lines_touched);
    } else {
        // If we scrolled, the whole screen changed, just do a full flip
        VESA_flip();
    }
}

void run_top() {
// 1. CLEAR the keyboard buffer so we don't process old keys
    while (has_key_in_buffer()) {
        get_key_from_buffer();
    }
    // Initial clear
    VESA_clear();
    
    while (1) {
        // Move cursor to 0,0 or clear
          vesa_updating = 1;         // Lock the timer out
        VESA_clear_buffer_only();


        kprintf_unsync("KDXOS TOP - System Ticks: %d\n", system_ticks);
        kprintf_unsync("Press 'q' to return to Shell\n");
        kprintf_unsync("-------------------------------------------\n");
        kprintf_unsync("TID   NAME         STATE      CPU-TICKS\n");

        for (int i = 0; i < MAX_TASKS; i++) {
            if (task_get_state(i) != 0) {
                // Print TID and Name
                kprintf_unsync("%d     %s", i, task_get_name(i));

                // Manual Padding for Name Column (since no %-13s)
                int name_len = kstrlen(task_get_name(i));
                for (int j = 0; j < (13 - name_len); j++) kprintf_unsync(" ");

                // Print State
                if (task_get_state(i) == 1)      kprintf_unsync("READY      ");
                else if (task_get_state(i) == 2) kprintf_unsync("SLEEP      ");

                // Print Ticks (We added this field to the task struct earlier)
                kprintf_unsync("%d\n", task_get_total_ticks(i));
            }
        }
        vesa_updating = 0;         // Unlock
        VESA_flip();               // Show finished frame


        // --- NON-BLOCKING CHECK ---
        // We use your io.c functions here
        if (has_key_in_buffer()) {
            char c = get_key_from_buffer(); 
            if (c == 'q' || c == 'Q') {
                break; // Exit the loop
            }
        }
               sleep(500); 
    }
    
    VESA_clear_buffer_only();
    kprintf_unsync("Returned to Shell.\n");
  VESA_flip();
}
