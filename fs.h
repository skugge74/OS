#ifndef FS_H
#define FS_H

#include <stdint.h>

#define MAX_FILES 32
#define MAX_FILENAME 16

typedef struct {
    char name[MAX_FILENAME];
    uint32_t size;
    uint32_t offset;  // Pointer to the actual data in RAM
    uint8_t  active;  // 1 if file exists, 0 if deleted
} file_entry_t;

void init_fs();
int kcreate_file(char* name, char* data);
void klist_files();

char* fs_get_name(int index);
uint32_t fs_get_size(int index);
char* fs_get_data(char* name);
int fs_is_active(int index);
int kwrite_to_file(char* name, char* new_data);
int kcreate_file_bin(char* name, unsigned char* data, uint32_t size);
#endif
