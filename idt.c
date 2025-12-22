#include "idt.h"
#include "vga.h"
#include "io.h"
#include "lib.h"
#include "task.h"

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
    VGA_clear();
    for(int i = 0; i < 80 * 25; i++) {
        ((uint16_t*)0xB8000)[i] = (uint16_t)' ' | (uint16_t)0x1F << 8;
    }
    cursor_pos = 0;

    kprintf_color(0x1F, "--- KERNEL PANIC ---\n");
    kprintf_color(0x1F, "Interrupt: %d (%s)\n", r->int_no, exception_messages[r->int_no]);
    kprintf_color(0x1F, "EIP: %x  EAX: %x  EBX: %x\n", r->eip, r->eax, r->ebx);
    kprintf_color(0x1F, "ECX: %x  EDX: %x\n", r->ecx, r->edx);
    
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
    // The PIT has an internal clock of 1.193182 MHz
    uint32_t divisor = 1193182 / frequency;

    // Send the command byte (0x36 sets square wave mode)
    outb(0x43, 0x36);

    // Split divisor into upper and lower bytes
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

    // Send the frequency divisor
    outb(0x40, l);
    outb(0x40, h);
}

void timer_handler(struct registers *regs) {
    system_ticks++;
    outb(0x20, 0x20);
    task_list[current_task_idx].esp = (uint32_t)regs;

    int next_task = (current_task_idx + 1) % MAX_TASKS;
    
    // Safety check: Count how many tasks we've checked to avoid infinite loops
    int check_count = 0;
    while (task_list[next_task].state != 1 && check_count < MAX_TASKS) {
        next_task = (next_task + 1) % MAX_TASKS;
        check_count++;
    }

    // If no other task is READY, but the current one was just KILLED (state 0)
    // we MUST force a switch to Task 0 (the Shell/Kernel)
    if (task_list[current_task_idx].state == 0 && next_task == current_task_idx) {
        next_task = 0; 
    }

    if (next_task != current_task_idx) {
        current_task_idx = next_task;
        uint32_t new_esp = task_list[next_task].esp;

        __asm__ volatile (
            "mov %0, %%esp \n"
            "pop %%eax     \n"
            "mov %%ax, %%ds \n"
            "mov %%ax, %%es \n"
            "popa          \n" 
            "add $8, %%esp \n"
            "iret          \n" 
            : : "r" (new_esp) : "memory"
        );
    }
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
    if (regs->eax == 1) { // Print Char At
        char c = (char)regs->ebx;
        int x = regs->ecx;
        int y = regs->edx;
        
        // --- General Cleanup Logic ---
        // Save coordinates in the PCB so kill_task knows where to wipe
        task_list[current_task_idx].last_x = x;
        task_list[current_task_idx].last_y = y;
        task_list[current_task_idx].has_drawn = 1;

        char str[2] = {c, '\0'};
        // 0x0E is Yellow on Black. (If your background is still blue, change to 0x1E)
        VGA_print_at(str, 0x0E, x, y); 
    } 
    else if (regs->eax == 2) { // Get Ticks
        regs->eax = system_ticks; 
    }
}
