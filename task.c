#include "task.h"
#include "vga.h"
#include <stdint.h>
#include "kheap.h"
#include "io.h"
#include "shell.h"
#include "lib.h"

#define MAX_TASKS 10

// External assembly function
extern void switch_to_stack(uint32_t* old_esp, uint32_t new_esp);
extern volatile uint32_t system_ticks;
// The Task Control Block (TCB) array
struct task task_list[MAX_TASKS];
int current_task_idx = 0;
void shell_task() {
    char line[128];
    int idx = 0;

    VGA_print("KDXOS Shell Started.\n", COLOR_CYAN);
    VGA_print("> ", COLOR_YELLOW);

    while(1) {
        char c = keyboard_getchar(); // This must call yield() internally if no key

        if (c == '\n') {
            line[idx] = '\0';
            VGA_print("\n", 0x07);
            if (idx > 0) {
                execute_command(line); 
            }
            idx = 0;
            VGA_print("> ", COLOR_YELLOW);
        } 
        else if (c == '\b' && idx > 0) {
            idx--;
            VGA_print("\b", 0x07);
        } 
        else if (idx < 127 && c >= ' ') {
            line[idx++] = c;
            char str[2] = {c, '\0'};
            VGA_print(str, 0x0F);
        }
    }
}
void kill_task(int id) {
    // 1. Validation: Don't kill the shell (0) or out-of-bounds IDs
    if (id <= 0 || id >= MAX_TASKS) return;

    // 2. Clear the spot on screen where the task last drew
    // We use the coordinates saved by the syscall_handler
    if (task_list[id].has_drawn) {
        int x = task_list[id].last_x;
        int y = task_list[id].last_y;
        
        // We call our existing VGA function to keep logic consistent
        // 0x07 is the standard "light grey on black" attribute
        VGA_print_at(" ", 0x07, x, y);
    }

    // 3. Mark as dead
    // The scheduler will now bypass this task slot
    task_list[id].state = 0;

    // 4. Reset task metadata for the next time this slot is used
    task_list[id].has_drawn = 0;
    task_list[id].last_x = 0;
    task_list[id].last_y = 0;

    // Optional: Print a status message to the shell
    // VGA_print("Task killed and spot cleared.\n", 0x0F);
}
// Stacks for the tasks (10 stacks of 4KB each)
uint32_t task_stacks[MAX_TASKS][1024];

// Correct yield loop logic
void yield() {
    __asm__ volatile("int $0x20"); // Trigger the Timer Interrupt manually
}

int spawn_task(void (*entry_point)()) {
    for (int i = 1; i < MAX_TASKS; i++) {
        if (task_list[i].state == 0) {
            // --- NEW: Reset drawing metadata ---
            // This ensures kill_task doesn't wipe a random spot 
            // from a previous process that lived in this slot.
            task_list[i].has_drawn = 0;
            task_list[i].last_x = 0;
            task_list[i].last_y = 0;

            // Allocate 4KB stack
            uint32_t* stack = (uint32_t*)kmalloc_a(4096);
            uint32_t* stack_ptr = stack + 1024; 

            // --- STEP 1: THE IRET FRAME ---
            *--stack_ptr = 0x10;                // SS
            *--stack_ptr = (uint32_t)(stack + 1024); // ESP
            *--stack_ptr = 0x202;               // EFLAGS (Interrupts Enabled)
            *--stack_ptr = 0x08;                // CS
            *--stack_ptr = (uint32_t)entry_point; // EIP

            // --- STEP 2: THE STUB DATA ---
            *--stack_ptr = 0;                   // err_code
            *--stack_ptr = 0;                   // int_no

            // --- STEP 3: THE PUSHA FRAME ---
            *--stack_ptr = 0;                   // eax
            *--stack_ptr = 0;                   // ecx
            *--stack_ptr = 0;                   // edx
            *--stack_ptr = 0;                   // ebx
            *--stack_ptr = 0;                   // esp (ignored)
            *--stack_ptr = 0;                   // ebp
            *--stack_ptr = 0;                   // esi
            *--stack_ptr = 0;                   // edi

            // --- STEP 4: THE SEGMENT SELECTOR ---
            *--stack_ptr = 0x10;                // ds

            task_list[i].esp = (uint32_t)stack_ptr;
            task_list[i].state = 1; // READY
            
            return i;
        }
    }
    return -1;
}

void init_multitasking() {
    // 1. Clear ALL slots first to be safe
    for (int i = 0; i < MAX_TASKS; i++) {
        task_list[i].state = 0;
        task_list[i].esp = 0;
    }

    // 2. Define Task 0 as the Shell (current execution)
    task_list[0].state = 1; 
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

int spawn_external_task(void* code_location) {
    // This is essentially the same as your current spawn_task, 
    // but we treat the pointer as the entry point.
    return spawn_task((void(*)())code_location);
}
