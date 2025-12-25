section .multiboot
align 4
    dd 0x1BADB002             ; magic
    dd 0x00000005             ; flags (Align modules + Graphics)
    dd -(0x1BADB002 + 0x00000005)

    ; Graphics parameters
    dd 0, 0, 0, 0, 0
    dd 0                      ; 0 = Linear Framebuffer
    dd 1024                   ; Width
    dd 768                    ; Height
    dd 32                     ; BPP (Bits Per Pixel)

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KiB
stack_top:

section .text
global _start:function (_start.end - _start)
_start:
    ; 1. Setup Stack
    mov esp, stack_top
; SAVE the Multiboot registers before the zeroing loop destroys them
    push ebx                ; Save MBI pointer
    push eax                ; Save Magic number
    ; 2. Clear the BSS section (Zero out uninitialized globals)
    ; This fixes the "Million Ticks" bug by ensuring system_ticks starts at 0
    extern __bss_start
    extern _end
    
    mov edi, __bss_start    ; Starting address of BSS
    mov ecx, _end           ; End address of BSS
    sub ecx, edi            ; Calculate size of BSS
    xor eax, eax            ; Value to write (0)
    rep stosb               ; Fill memory with zeros

    ; 3. Push Multiboot arguments for kmain(magic, info_ptr)
    push ebx                ; Multiboot info structure
    push eax                ; Magic Number

    extern kmain
    call kmain

    ; 4. Halt if kmain returns
    cli
.hang:
    hlt
    jmp .hang
.end:
