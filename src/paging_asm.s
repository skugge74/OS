global load_page_directory
load_page_directory:
    push ebp
    mov ebp, esp
    mov eax, [ebp+8] ; Get the pointer to the directory
    mov cr3, eax     ; Load CR3 with the directory address
    mov esp, ebp
    pop ebp
    ret

global enable_paging
enable_paging:
    push ebp
    mov ebp, esp
    mov eax, cr0
    or eax, 0x80000001 ; Set the PG (Paging) and PE (Protection) bits
    mov cr0, eax
    mov esp, ebp
    pop ebp
    ret
