#include "fs.h"
#include "kheap.h"
#include "lib.h"
#include "vga.h"

file_entry_t files[MAX_FILES];

unsigned char spinner_code[] = {
    // 1. Get Ticks (Syscall 2)
    0xB8, 0x02, 0x00, 0x00, 0x00, // [0]  MOV EAX, 2
    0xCD, 0x80,                   // [5]  INT 0x80 -> EAX = ticks

    // 2. Slow down and get index (0-3)
    0xC1, 0xE8, 0x05,             // [7]  SHR EAX, 5 (Slow rotation)
    0x83, 0xE0, 0x03,             // [10] AND EAX, 3 (Result is 0, 1, 2, or 3)

    // 3. The "String" Lookup
    // We store the characters "-\|/" in a 32-bit register.
    // ASCII: '-'=0x2D, '\'=0x5C, '|'=0x7C, '/'=0x2F
    0xBB, 0x2D, 0x5C, 0x7C, 0x2F, // [13] MOV EBX, 0x2F7C5C2D

    // 4. Shift EBX right by (EAX * 8) bits to bring the character to BL
    0x88, 0xC1,                   // [18] MOV CL, AL (CL = 0, 1, 2, or 3)
    0xC1, 0xE1, 0x03,             // [20] SHL CL, 3  (CL = 0, 8, 16, or 24)
    0xD3, 0xEB,                   // [23] SHR EBX, CL (Shift chosen char into BL)
    0x81, 0xE3, 0xFF, 0x00, 0x00, 0x00, // [25] AND EBX, 0xFF (Clear upper bits)

    // 5. Print (Syscall 1)
    0xB8, 0x01, 0x00, 0x00, 0x00, // [31] MOV EAX, 1
    0xB9, 0x4F, 0x00, 0x00, 0x00, // [36] MOV ECX, 79 (X)
    0xBA, 0x00, 0x00, 0x00, 0x00, // [41] MOV EDX, 0  (Y)
    0xCD, 0x80,                   // [46] INT 0x80

    // 6. Yield
    0xCD, 0x20,                   // [48] INT 0x20

    // 7. Loop back to index 0
    0xEB, 0xCC                    // [50] JMP -52 bytes
    // Total size: 52 bytes. 52 - 52 = 0.
};

unsigned char clock_code[] = {
    // 1. Get Ticks (Syscall 2)
    0xB8, 0x02, 0x00, 0x00, 0x00, // [0]  MOV EAX, 2
    0xCD, 0x80,                   // [5]  INT 0x80 -> EAX = ticks

    // 2. Extract a "Slow" nibble (4 bits)
    // Shifting right by 7 means the number changes every 128 ticks (~1.3s)
    0xC1, 0xE8, 0x07,             // [7]  SHR EAX, 7
    0x83, 0xE0, 0x0F,             // [10] AND EAX, 0x0F (Keep 0-15)

    // 3. Keep it within 0-9 for now
    0xBB, 0x0A, 0x00, 0x00, 0x00, // [13] MOV EBX, 10
    0x31, 0xD2,                   // [18] XOR EDX, EDX
    0xF7, 0xF3,                   // [20] DIV EBX (Remainder in EDX is 0-9)

    // 4. Convert Digit to ASCII
    0x83, 0xC2, 0x30,             // [22] ADD EDX, 0x30
    0x89, 0xD3,                   // [25] MOV EBX, EDX

    // 5. Print (Syscall 1)
    0xB8, 0x01, 0x00, 0x00, 0x00, // [27] MOV EAX, 1
    0xB9, 0x4E, 0x00, 0x00, 0x00, // [32] MOV ECX, 78 (X pos)
    0xBA, 0x00, 0x00, 0x00, 0x00, // [37] MOV EDX, 0  (Y pos)
    0xCD, 0x80,                   // [42] INT 0x80

    // 6. Yield
    0xCD, 0x20,                   // [44] INT 0x20

    // 7. Loop Back
    0xEB, 0xD0                    // [46] JMP -48 bytes (Back to index 0)
    // Size: 48 bytes.
};

void init_fs() {
    for(int i = 0; i < MAX_FILES; i++) {
        files[i].active = 0;
    }
  kcreate_file_bin("clock.bin", clock_code, sizeof(clock_code));
  kcreate_file_bin("spinner.bin", spinner_code, sizeof(spinner_code));
}
int kcreate_file_bin(char* name, unsigned char* data, uint32_t size) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].active == 0) {
            // 1. Copy filename safely
            kstrncpy(files[i].name, name, MAX_FILENAME - 1);
            files[i].name[MAX_FILENAME - 1] = '\0';
            
            files[i].size = size;
            
            // 2. Allocate the exact amount of RAM needed
            unsigned char* file_content = (unsigned char*)kmalloc_a(size);
            // 3. Binary Safe Copy: Manual byte-by-byte transfer
            for (uint32_t j = 0; j < size; j++) {
                file_content[j] = data[j];
            }
            
            // 4. Set the metadata
            files[i].offset = (uint32_t)file_content;
            files[i].active = 1;
            
            return i; // Return the file index
        }
    }
    return -1; // No room in root directory
}
int kwrite_to_file(char* name, char* new_data) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].active && kstrcmp(files[i].name, name) == 0) {
            // Calculate new size
            uint32_t new_size = kstrlen(new_data);
            
            // In a simple placement heap, we can't 'free' the old memory,
            // so we just allocate a new block for the updated content.
            char* new_ptr = (char*)kmalloc(new_size + 1);
            kstrcpy(new_ptr, new_data);
            
            files[i].offset = (uint32_t)new_ptr;
            files[i].size = new_size;
            return 0;
        }
    }
    // If file doesn't exist, create it
    return kcreate_file(name, new_data);
}
int kcreate_file(char* name, char* data) {
    for(int i = 0; i < MAX_FILES; i++) {
        if(files[i].active == 0) {
            kstrncpy(files[i].name, name, MAX_FILENAME);
            files[i].size = kstrlen(data);
            
            // Allocate space for the file data
            char* file_data = (char*)kmalloc(files[i].size + 1);
            kstrcpy(file_data, data);
            
            files[i].offset = (uint32_t)file_data;
            files[i].active = 1;
            return 0; // Success
        }
    }
    return -1; // Disk Full
}

int fs_is_active(int index) {
    if (index < 0 || index >= MAX_FILES) return 0;
    return files[index].active;
}

char* fs_get_name(int index) {
    return files[index].name;
}

uint32_t fs_get_size(int index) {
    return files[index].size;
}

char* fs_get_data(char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].active && kstrcmp(files[i].name, name) == 0) {
            return (char*)files[i].offset; // Ensure this is a direct pointer
        }
    }
    return 0;
}
