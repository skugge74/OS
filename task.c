#include "task.h"
#include "vesa.h"
#include <stdint.h>
#include "kheap.h"
#include "io.h"
#include "shell.h"
#include "lib.h"

#define MAX_TASKS 10

// External assembly function
extern void switch_to_stack(uint32_t* old_esp, uint32_t new_esp);
extern volatile uint32_t system_ticks;
int multitasking_enabled = 0;
// The Task Control Block (TCB) array
volatile struct task task_list[MAX_TASKS];
int current_task_idx = 0;
void shell_task() {
    char line[128];
    int idx = 0;

    VESA_print("KDXOS Shell Started.\n", COLOR_CYAN);
    VESA_print("> ", COLOR_YELLOW);

    while(1) {
        char c = keyboard_getchar(); // This must call yield() internally if no key

        if (c == '\n') {
            line[idx] = '\0';
            VESA_print("\n", COLOR_WHITE);
            if (idx > 0) {
                execute_command(line); 
            }
            idx = 0;
            VESA_print("> ", COLOR_YELLOW);
        } 
        else if (c == '\b' && idx > 0) {
            // backspace and non empty line
            idx--;
            if (vesa_cursor_x >= 8) {
              vesa_cursor_x -= 8;
            }
            VESA_draw_char(' ', vesa_cursor_x, vesa_cursor_y, 0x222222); 
        } 
        else if (idx < 127 && c >= ' ') {
            line[idx++] = c;
            char str[2] = {c, '\0'};
            VESA_print(str, COLOR_WHITE);
        }
    }
}

int spawn_task(void (*entry_point)(), void* code_ptr, char* name) {
    for (int i = 1; i < MAX_TASKS; i++) {
        if (task_list[i].state == 0) {
            // 1. Reset Metadata & Set Name
            task_list[i].has_drawn = 0;
            task_list[i].last_x = 0;
            task_list[i].last_y = 0;
            
            // Initialize CPU Accounting and Sleep state ---
            task_list[i].total_ticks = 0;    // Reset CPU odometer
            task_list[i].sleep_ticks = 0;    // Ensure it doesn't start asleep
      
            // Copy name safely
            kstrncpy((char*)task_list[i].name, name, 15);
            task_list[i].name[15] = '\0'; 

            // 2. Allocate RAW memory (Store this for kfree)
            // We allocate 8KB to ensure we can find a 4KB aligned block inside
            void* raw_stack = kmalloc(4096 + 0x1000);
            if (!raw_stack) return -1;

            task_list[i].stack_ptr = raw_stack; 
            task_list[i].code_ptr = code_ptr; 

            // 3. Calculate the ALIGNED stack for the CPU
            uint32_t aligned_stack = (uint32_t)raw_stack;
            if (aligned_stack & 0xFFF) {
                aligned_stack &= 0xFFFFF000;
                aligned_stack += 0x1000;
            }

            // 4. Build the stack frame at the TOP of the aligned 4KB
            uint32_t* s_ptr = (uint32_t*)(aligned_stack + 4096);

            // --- THE IRET FRAME ---
            *--s_ptr = 0x10;                     // SS
            uint32_t task_stack_top = (uint32_t)s_ptr; 
            *--s_ptr = task_stack_top;           // ESP
            *--s_ptr = 0x202;                    // EFLAGS
            *--s_ptr = 0x08;                     // CS
            *--s_ptr = (uint32_t)entry_point;    // EIP

            // --- INT DATA (Matches irq0_handler) ---
            *--s_ptr = 0;                        // err_code
            *--s_ptr = 32;                       // int_no (Timer)

            // --- PUSHA FRAME ---
            for(int j = 0; j < 8; j++) *--s_ptr = 0;

            // --- DATA SEGMENT ---
            *--s_ptr = 0x10;                     // DS

            // 5. Save final ESP and set to READY
            task_list[i].esp = (uint32_t)s_ptr;
            task_list[i].state = 1; 
            
            return i;
        }
    }
    return -1;
}

// Stacks for the tasks (10 stacks of 4KB each)
uint32_t task_stacks[MAX_TASKS][1024];

// Correct yield loop logic
void yield() {
    __asm__ volatile("int $0x20"); // Trigger the Timer Interrupt manually
}

void kill_task(int id) {
    if (id <= 0 || id >= MAX_TASKS) return;

    // 1. Clear visual metadata (Your original logic)
    if (task_list[id].has_drawn) {
        VESA_draw_char(' ', task_list[id].last_x, task_list[id].last_y, 0x222222);
    }

    // 2. RELEASE MEMORY (The Fix)
    // Free the stack
    if (task_list[id].stack_ptr != NULL) {
        kfree(task_list[id].stack_ptr);
        task_list[id].stack_ptr = NULL;
    }

    // Free the code image (allocated in the RUN command)
    if (task_list[id].code_ptr != NULL) {
        kfree(task_list[id].code_ptr);
        task_list[id].code_ptr = NULL;
    }

    // 3. Mark as dead
    task_list[id].state = 0;
    task_list[id].has_drawn = 0;
    task_list[id].last_x = 0;
    task_list[id].last_y = 0;
}
void idle_task_code() {
    while(1) {
        __asm__ volatile("hlt");
    }
}
void init_multitasking() {
    multitasking_enabled = 1;
    for (int i = 0; i < MAX_TASKS; i++) task_list[i].state = 0;

    // Task 0: Shell
    task_list[0].state = 1;
    kstrncpy((char* )task_list[0].name, "shell", 15);
    
    // Task 9: Idle Task (Always READY)
    // Use your existing spawn_task logic or manually set it up
    spawn_task(idle_task_code, NULL, "idle");
    
    current_task_idx = 0;
}
// Helper function
int get_current_task_id() {
    return current_task_idx;
}

int task_is_ready(int id) {
    if (id < 0 || id >= MAX_TASKS) return 0;
    return task_list[id].state == 1;
}

uint32_t task_get_esp(int id) {
    if (id < 0 || id >= MAX_TASKS) return 0;
    return task_list[id].esp;
}

char* task_get_name(int id) {
    if (id < 0 || id >= MAX_TASKS) return "unused";
    return (char*)task_list[id].name;
}

int task_get_state(int id){
    if (id < 0 || id >= MAX_TASKS) return -1;
    return task_list[id].state;
}
int task_get_sleep_ticks(int id){
  if (id < 0 || id >= MAX_TASKS) return -1;
  return task_list[id].sleep_ticks;
}
int task_get_total_ticks(int id){
  if (id < 0 || id >= MAX_TASKS) return -1;
  return task_list[id].total_ticks;
}
void task_timer() {
    uint32_t seconds = 0;
    while (1) {
        seconds++;

        char buf[20];
        kmemset(buf, 0, 20);
        kstrcpy(buf, "TIMER: ");
        itoa(seconds, buf + 7, 10); 
        VESA_print_at(buf, 900, 10, 0x00FFFF); 

        sleep(1000); 
    }
}
