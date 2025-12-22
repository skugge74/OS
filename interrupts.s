; --- Macros for Processor Exceptions ---
%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    push byte 0
    push byte %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
  global isr%1
  isr%1:
    push byte %1
    jmp isr_common_stub
%endmacro

; Define the 32 standard CPU exceptions
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_ERRCODE   21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31

[extern isr_handler]

; This stub is used by all CPU exceptions
isr_common_stub:
    pusha               ; Pushes edi, esi, ebp, esp, ebx, edx, ecx, eax
    mov ax, ds
    push eax            ; Save the current data segment

    mov ax, 0x10        ; Load the Kernel Data Segment (0x10)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp            ; Pass pointer to the stack (registers_t)
    call isr_handler
    
    add esp, 4
    pop eax             ; Restore data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8          ; Clean up error code and isr number
    iret

; --- Hardware IRQ Handlers ---
; At the top of your file with other externs
extern timer_handler
extern keyboard_handler
extern next_stack_ptr    ; We assume this is defined in idt.c or task.c

global irq0_handler
irq0_handler:
    push byte 0         ; Dummy error code
    push byte 32        ; Interrupt number
    pusha               ; Save EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX
    
    mov ax, ds          ; Save current Data Segment
    push eax            

    mov ax, 0x10        ; Load Kernel Data Segment
    mov ds, ax
    mov es, ax

    push esp            ; Pass the current stack frame to the C handler
    call timer_handler
    add esp, 4          ; Clean up the pushed ESP

    ; --- THE TASK SWITCH LOGIC ---
    ; After timer_handler, next_stack_ptr contains the ESP of the next task
    mov eax, [next_stack_ptr]
    test eax, eax       ; Faster check for NULL
    jz .no_switch
    mov esp, eax        ; ACTUAL SWITCH: Change the CPU stack pointer
.no_switch:

    pop eax             ; Restore Data Segment
    mov ds, ax
    mov es, ax
    
    popa                ; Restore registers for the NEW task
    add esp, 8          ; Clean up error code and int_no
    iret                ; Exit interrupt and jump to the new task's EIP

global irq1_handler
irq1_handler:
    push byte 0         
    push byte 33        
    pusha
    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax

    push esp
    call keyboard_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    popa
    add esp, 8          
    iret
; --- System Call Handler (int 0x80) ---

[extern syscall_handler]
global isr128_stub
isr128_stub:
    push byte 0         ; Dummy error code
    push dword 128      ; Interrupt number
    pusha               ; Save all registers (EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX)
    
    mov ax, ds
    push eax            ; Save data segment

    mov ax, 0x10        ; Kernel Data Segment
    mov ds, ax
    mov es, ax

    push esp            ; Pass the registers to C
    call syscall_handler
    
    ; After the C call, the new EAX value is in the physical EAX register.
    ; We must overwrite the EAX value on the stack so popa picks it up.
    ; Stack layout right now:
    ; [esp + 0]  = Saved DS
    ; [esp + 4]  = EDI
    ; [esp + 8]  = ESI
    ; [esp + 12] = EBP
    ; [esp + 16] = ESP (original)
    ; [esp + 20] = EBX
    ; [esp + 24] = EDX
    ; [esp + 28] = ECX
    ; [esp + 32] = EAX <--- This is the one we want to overwrite!
    
    mov [esp + 32], eax 

    add esp, 4          ; Clean up pointer
    
    pop eax             ; Restore data segment
    mov ds, ax
    mov es, ax

    popa                ; This loads the MODIFIED EAX from the stack!
    add esp, 8          ; Clean up error code and int number
    iret

; --- Utility Functions ---

global idt_flush
idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret

global switch_to_stack
switch_to_stack:
    push ebp
    push edi
    push esi
    push ebx

    mov eax, [esp + 20] ; old_esp pointer
    mov ecx, [esp + 24] ; new_esp value
    
    mov [eax], esp      ; save old esp
    mov esp, ecx        ; load new esp

    pop ebx
    pop esi
    pop edi
    pop ebp
    ret
