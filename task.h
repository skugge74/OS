#ifndef TASK_H
#define TASK_H
#include <stdint.h>

#define MAX_TASKS 10
#define STACK_SIZE 4096

struct task {
    uint32_t esp;
    uint32_t state; // 0 = empty, 1 = ready
    char name[16];
    uint32_t vga_index; // Store the character position (0-3999)
    int last_x; // Track the X coordinate used in syscall
    int last_y; // Track the Y coordinate used in syscall
    int has_drawn; // Boolean flag: did this task ever print?
    void* stack_ptr; // Store this so we can kfree it!
    void* code_ptr;  // Store this so we can kfree it!
};

void init_multitasking();
void yield();
int get_current_task_id();
int spawn_task(void (*entry_point)(), void* code_ptr, char* name);
void kill_task(int id);
uint32_t task_get_esp(int id);
int task_is_ready(int id);
void shell_task();
char* task_get_name(int id);
#endif
