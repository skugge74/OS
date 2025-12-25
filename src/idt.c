#include "idt.h"
#include "io.h"
#include "lib.h"
#include "task.h"
#include "vesa.h"

uint32_t timer_frequency = 0; // Global variable to store the frequency
extern struct task task_list[];
extern int current_task_idx;
extern void isr0(); // Declaration of the assembly label
volatile uint32_t system_ticks = 0;
struct idt_entry idt[256];
struct idt_ptr idtp;
extern void isr128_stub();
int shift_state = 0; // Add this line to idt.c
char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    // ... add more if you like
};


// Ensure your 'struct registers' is defined in idt.h exactly 
// as the stack was pushed in assembly!
void isr_handler(struct registers *r) {
    VESA_clear();
    for(int i = 0; i < 80 * 25; i++) {
        ((uint16_t*)0xB8000)[i] = (uint16_t)' ' | (uint16_t)0x1F << 8;
    }
    vesa_cursor_x = 0;
    vesa_cursor_y = 0;


    kprintf_color(COLOR_WHITE, "--- KERNEL PANIC ---\n");
    kprintf_color(COLOR_WHITE, "Interrupt: %d (%s)\n", r->int_no, exception_messages[r->int_no]);
    kprintf_color(COLOR_WHITE, "EIP: %x  EAX: %x  EBX: %x\n", r->eip, r->eax, r->ebx);
    kprintf_color(COLOR_WHITE, "ECX: %x  EDX: %x\n", r->ecx, r->edx);
    
    while(1) __asm__("hlt");
}

// In idt_init, map the first entry

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}


extern void irq1_handler(); // The assembly function

extern char buffer[128];
extern int buffer_idx;
extern int shift_state;


char kbd_buffer[KEYBOARD_BUFFER_SIZE];
int kbd_head = 0;
int kbd_tail = 0;

void irq_handler() {
    uint8_t scancode = inb(0x60);

    if (scancode == 0x2A || scancode == 0x36) {
        shift_state = 1;
    } else if (scancode == 0xAA || scancode == 0xB6) {
        shift_state = 0;
    } 
    else if (!(scancode & 0x80)) {
        char c = scancode_to_ascii(scancode, shift_state);
        if (c != 0) {
            // Push into buffer
            int next = (kbd_head + 1) % KEYBOARD_BUFFER_SIZE;
            if (next != kbd_tail) { // Check if buffer is full
                kbd_buffer[kbd_head] = c;
                kbd_head = next;
            }
        }
    }
    outb(0x20, 0x20);
}

void pic_remap() {
    outb(0x20, 0x11); // Initialize Master PIC
    outb(0xA0, 0x11); // Initialize Slave PIC
    outb(0x21, 0x20); // Map Master to 0x20 (32)
    outb(0xA1, 0x28); // Map Slave to 0x28 (40)
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF); // Mask all Slave IRQs
}



void idt_init() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;
    // Clear the table
    for(int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    idt_set_gate(128, (uint32_t)isr128_stub, 0x08, 0x8E);
    // ADD THIS: Handle the Timer (IRQ 0 -> INT 32)
    extern void irq0_handler();
    idt_set_gate(32, (uint32_t)irq0_handler, 0x08, 0x8E);
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    extern void isr13();
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E); // Register GPF handler

    // Keep your Keyboard (IRQ 1 -> INT 33)
    extern void irq1_handler();
    idt_set_gate(33, (uint32_t)irq1_handler, 0x08, 0x8E);

    __asm__ volatile("lidt (%0)" : : "r" (&idtp));
}





void timer_init(uint32_t frequency) {
    if (frequency == 0) frequency = 1; // Prevent division by zero
    timer_frequency = frequency; 

    uint32_t divisor = 1193182 / frequency;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

// Define this at the top of idt.c
uint32_t next_stack_ptr = 0;
extern uint32_t target_fps;
void timer_handler(struct registers *regs) {
    system_ticks++;
    // Calculate how many ticks must pass for one frame
    // Example: 1000Hz / 60 FPS = 16 ticks
    uint32_t ticks_per_frame = timer_frequency / target_fps;

    // Safety: Ensure we don't divide by zero or get a 0 interval
    if (ticks_per_frame == 0) ticks_per_frame = 1;

    // Now it updates based on your variable!
    if (system_ticks % ticks_per_frame == 0) {
        VESA_flip();
    }if (multitasking_enabled){
    // --- NEW: CPU Accounting ---
    // Increment the tick count for the task that was just interrupted.
    // This tracks how much actual CPU time each process is getting.
    if (task_list[current_task_idx].state != 0) {
        task_list[current_task_idx].total_ticks++;
    }

    // 1. Update sleeping tasks
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_list[i].state == 2) { // 2 = SLEEPING
            if (task_list[i].sleep_ticks > 0) {
                task_list[i].sleep_ticks--;
            } 
            // Check again after decrementing to wake up immediately if time is up
            if (task_list[i].sleep_ticks == 0) {
                task_list[i].state = 1; // Wake up! Set to READY
            }
        }
    }

    // 2. Save current task ESP
    // This stores the stack pointer so we can resume this task later
    task_list[current_task_idx].esp = (uint32_t)regs;

    // 3. Find NEXT task that is READY (state 1)
    int next_task = (current_task_idx + 1) % MAX_TASKS;
    int check_count = 0;
    while (task_list[next_task].state != 1 && check_count < MAX_TASKS) {
        next_task = (next_task + 1) % MAX_TASKS;
        check_count++;
    }

    // Update global pointers for the Assembly stub to perform the switch
    current_task_idx = next_task;
    next_stack_ptr = task_list[current_task_idx].esp;
  }
    // Send End of Interrupt (EOI) to the PIC
    outb(0x20, 0x20);
}
// Track shift state globally in idt.c or io.c
static int shift_pressed = 0;

void keyboard_handler(struct registers *regs) {
    (void)regs; // Silences the 'unused parameter' warning
    
    uint8_t scancode = inb(0x60);

    // Check for Shift Key Press (Make codes)
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
    }
    // Check for Shift Key Release (Break codes: scancode + 0x80)
    else if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
    }
    // Process actual key presses
    else if (!(scancode & 0x80)) {
        // Pass both the scancode and the shift state
        char c = scancode_to_ascii(scancode, shift_pressed);
        
        if (c > 0) {
            keyboard_push_char(c); 
        }
    }

    // Tell the PIC we are done
    outb(0x20, 0x20);
}
void syscall_handler(struct registers *regs) {
    if (regs->eax == 1) { // Print Char
        char c = (char)regs->ebx;
        int x = regs->ecx;
        int y = regs->edx;
        
        // Use your VESA function to draw it!
        VESA_draw_char(c, x, y, 0xFFFFFF); 
    } 
    else if (regs->eax == 2) { // Get Ticks
        regs->eax = system_ticks; 
    }

else if (regs->eax == 3) { // Syscall 3: Sleep(ms)
    uint32_t ms = regs->ebx;
    
    // 1. Convert Milliseconds to Ticks based on current frequency
    // Formula: Ticks = (ms * frequency) / 1000
    uint32_t ticks_to_sleep = (ms * timer_frequency) / 1000;

    // Safety: Ensure we sleep at least 1 tick if ms > 0
    if (ms > 0 && ticks_to_sleep == 0) {
        ticks_to_sleep = 1;
    }

    task_list[current_task_idx].sleep_ticks = ticks_to_sleep; 
    task_list[current_task_idx].state = 2; // Set state to SLEEPING
    task_list[current_task_idx].esp = (uint32_t)regs;

    // 2. Find the next READY task
    int next_task = (current_task_idx + 1) % MAX_TASKS;
    int found = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        int idx = (next_task + i) % MAX_TASKS;
        if (task_list[idx].state == 1) { // Found a READY task
            found = idx;
            break;
        }
    }

    // 3. SAFETY: If NO other task is ready (e.g., shell is alone and no idle task),
    // we cannot put the current task to sleep or the CPU will have nothing to run.
    if (found == -1 || found == current_task_idx) {
        task_list[current_task_idx].state = 1; // Keep it READY
        next_stack_ptr = 0;                    // Don't switch stacks
        return; 
    }

    // 4. Perform the switch
    current_task_idx = found;
    next_stack_ptr = task_list[found].esp;
}

}
