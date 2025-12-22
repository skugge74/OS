#ifndef IDT
#define IDT


#include <stdint.h>

struct idt_entry {
    uint16_t base_low;    // Lower 16 bits of handler function address
    uint16_t sel;         // Kernel segment selector
    uint8_t  always0;     // This must always be zero
    uint8_t  flags;       // Flags (Presence, Privilege, Type)
    uint16_t base_high;   // Upper 16 bits of handler function address
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct registers {
    uint32_t ds;                                     // Data segment (pushed last)
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; // Pushed by PUSHA
    uint32_t int_no, err_code;                       // Pushed by assembly stub
    uint32_t eip, cs, eflags, useresp, ss;           // Pushed by CPU
};void idt_init();
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void irq_handler();
void pic_remap(); 
void timer_handler();
void idt_init();
void timer_handler();
void timer_init(uint32_t frequency);
void keyboard_handler(struct registers *regs); 
void syscall_handler(struct registers *regs);

#endif // !IDT
