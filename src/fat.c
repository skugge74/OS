#include "ide.h"
#include "lib.h"
#include "fat.h"
#include <stdint.h>
#include "kheap.h"
#include "io.h"
#include "lib.h"
#include "vesa.h"

static struct fat_bpb bpb;
static uint32_t root_dir_sectors;
static uint32_t first_data_sector;
static uint32_t first_fat_sector;
static uint32_t current_dir_cluster = 0; // 0 means Root Directory

int fat_compare_name(const char* input, char* fat_name, char* fat_ext) {
    // 1. HARD MATCH for "." and ".."
    // We check the first two bytes of fat_name directly
    if (input[0] == '.' && input[1] == '\0') {
        return (fat_name[0] == '.' && fat_name[1] == ' ');
    }
    if (input[0] == '.' && input[1] == '.' && input[2] == '\0') {
        return (fat_name[0] == '.' && fat_name[1] == '.');
    }

    // 2. Regular 8.3 Comparison (for everything else)
    char clean_name[13];
    // ... rest of your logic ...
    int p = 0;

    // Build Name part
    for (int i = 0; i < 8 && fat_name[i] != ' '; i++) {
        clean_name[p++] = fat_name[i];
    }
    
    // Build Extension part (if it exists)
    if (fat_ext[0] != ' ') {
        clean_name[p++] = '.';
        for (int i = 0; i < 3 && fat_ext[i] != ' '; i++) {
            clean_name[p++] = fat_ext[i];
        }
    }
    clean_name[p] = '\0';

    // 3. CASE-INSENSITIVE COMPARE
    // FAT usually stores names in UPPERCASE. 
    // If you type "cd lol", you need to match "LOL".
    return (kstrcasecmp(input, clean_name) == 0); 
}

uint32_t get_current_dir_lba() {
    // If we are at cluster 0, we must jump to the Root Directory start
    if (current_dir_cluster == 0) {
        return first_fat_sector + (bpb.num_fats * bpb.fat_size_16);
    }
    // Otherwise, calculate the LBA of the data cluster
    return cluster_to_lba(current_dir_cluster);
}
void fat_init() {
    uint8_t sector0[512];
    ide_read_sector(0, sector0);
    kmemcpy(&bpb, sector0, sizeof(struct fat_bpb));

    // Calculate locations
    root_dir_sectors = ((bpb.root_entry_count * 32) + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;
    first_fat_sector = bpb.reserved_sector_count;
    uint32_t first_root_dir_sector = first_fat_sector + (bpb.num_fats * bpb.fat_size_16);
    first_data_sector = first_root_dir_sector + root_dir_sectors;
}

// Helper to convert Cluster to LBA
uint32_t cluster_to_lba(uint32_t cluster) {
    return ((cluster - 2) * bpb.sectors_per_cluster) + first_data_sector;
}
uint16_t fat_get_next_cluster(uint16_t cluster) {
    uint8_t fat_table[512];
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = first_fat_sector + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    ide_read_sector(fat_sector, fat_table);
    return *(uint16_t*)&fat_table[ent_offset];
}

void* fat_load_file(struct fat_dir_entry* entry) {
    uint32_t size = entry->size;
    uint8_t* buffer = kmalloc(size);
    uint16_t cluster = entry->first_cluster_low;
    uint32_t bytes_read = 0;

    while (cluster < 0xFFF8) { // Standard FAT16 End-of-Chain
        uint32_t lba = cluster_to_lba(cluster);
        for (int i = 0; i < bpb.sectors_per_cluster; i++) {
            ide_read_sector(lba + i, buffer + bytes_read);
            bytes_read += 512;
            if (bytes_read >= size) return buffer;
        }
        cluster = fat_get_next_cluster(cluster);
    }
    return buffer;
}

struct fat_dir_entry* fat_search(const char* filename) {
    static struct fat_dir_entry result;
    uint8_t buffer[512];
    
    // FIX: Use the helper that looks at current_dir_cluster!
    uint32_t search_lba = get_current_dir_lba();

    // Determine how many sectors to search
    // Root has a fixed size, subdirectories are (initially) 1 cluster
    uint32_t sectors_to_search = (current_dir_cluster == 0) ? root_dir_sectors : bpb.sectors_per_cluster;

    for (uint32_t s = 0; s < sectors_to_search; s++) {
        ide_read_sector(search_lba + s, buffer);
        struct fat_dir_entry* entries = (struct fat_dir_entry*)buffer;

        for (int i = 0; i < 16; i++) { // 16 entries per 512-byte sector
            if (entries[i].name[0] == 0x00) return NULL; // End of directory
            if ((unsigned char)entries[i].name[0] == 0xE5) continue; // Deleted
            if (entries[i].attr == 0x0F) continue; // Skip LFN junk

            if (fat_compare_name(filename, (char*)entries[i].name, (char*)entries[i].ext)) {
                kmemcpy(&result, &entries[i], sizeof(struct fat_dir_entry));
                return &result;
            }
        }
    }
    return NULL;
}

void fat_cd(const char* path) {
    struct fat_dir_entry* entry = fat_search(path);
    
    if (entry) {
        kprintf_unsync("Found '%s'! Target Cluster: %d, Attr: %x\n", 
                        path, entry->first_cluster_low, entry->attr);

        if (entry->attr & 0x10) {
            current_dir_cluster = entry->first_cluster_low;
            // Force a refresh of the shell logic
            kprintf_unsync("Moved to Cluster %d\n", current_dir_cluster);
        }
    } else {
        kprintf_unsync("CD: Could not find '%s'\n", path);
    }
}

void fat_ls() {
    uint8_t buffer[512];
    uint32_t dir_lba = get_current_dir_lba();
    ide_read_sector(dir_lba, buffer);

    struct fat_dir_entry* entry = (struct fat_dir_entry*)buffer;
    kprintf_unsync("Directory Listing:\n");

    for (int i = 0; i < 16; i++) {
        if (entry[i].name[0] == 0x00) break;
        if ((unsigned char)entry[i].name[0] == 0xE5) continue;
        if (entry[i].attr == 0x0F) continue; // Skip LFN junk

        if (entry[i].attr & 0x10) kprintf_unsync("[DIR] ");
        else kprintf_unsync("      ");

        // --- PRINT NAME ---
        for (int n = 0; n < 8; n++) {
            if (entry[i].name[n] != ' ') kputc(entry[i].name[n]);
        }

        // --- SMART CONDITIONAL DOT ---
        // 1. Don't print a dot if it's the "." or ".." directory entries
        // 2. Only print a dot if the extension has actual characters (not spaces)
        int is_dot_entry = (entry[i].name[0] == '.');
        
        if (!is_dot_entry) {
            if (entry[i].ext[0] != ' ' || entry[i].ext[1] != ' ' || entry[i].ext[2] != ' ') {
                kputc('.');
                for (int e = 0; e < 3; e++) {
                    if (entry[i].ext[e] != ' ') kputc(entry[i].ext[e]);
                }
            }
        }

        kprintf_unsync("  %d bytes\n", entry[i].size);
    }
    VESA_flip();
}

uint32_t fat_get_current_cluster() {
    return current_dir_cluster;
}
void fat_print_fixed(const char* str, int len) {
    for (int i = 0; i < len; i++) {
        // FAT pads with spaces (0x20). 
        // We print the char as long as it isn't a space.
        if (str[i] != ' ') {
            // Use your low-level char print (likely VESA_draw_char or similar)
            kputc(str[i]); 
        }
    }
}
void fat_print_name_ext(unsigned char* name, unsigned char* ext) {
    // Print the 8-character name
    for (int i = 0; i < 8; i++) {
        if (name[i] != ' ') {
            kputc(name[i]);
        }
    }

    // Only print the dot if there is an extension
    if (ext[0] != ' ') {
        kputc('.');
        for (int i = 0; i < 3; i++) {
            if (ext[i] != ' ') {
                kputc(ext[i]);
            }
        }
    }
}
uint16_t fat_find_free_cluster() {
    uint8_t fat_buffer[512];
    // Search the FAT table (starts at first_fat_sector)
    for (uint32_t s = 0; s < bpb.fat_size_16; s++) {
        ide_read_sector(first_fat_sector + s, fat_buffer);
        uint16_t* entries = (uint16_t*)fat_buffer;
        
        for (int i = 0; i < 256; i++) {
            if (entries[i] == 0x0000) { // 0x0000 means free
                return (s * 256) + i;
            }
        }
    }
    return 0xFFFF; // Disk full
}

void fat_update_table(uint16_t cluster, uint16_t value) {
    uint8_t fat_buffer[512];
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = first_fat_sector + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    ide_read_sector(fat_sector, fat_buffer);
    *(uint16_t*)&fat_buffer[ent_offset] = value;
    ide_write_sector(fat_sector, fat_buffer); // You'll need ide_write_sector!
}

void ide_write_sector(uint32_t lba, uint8_t* buffer) {
    // 1. Wait for BSY to clear
    while (inb(0x1F7) & 0x80); 

    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30); // Write Command

    // 2. Wait for BSY to clear AND DRQ to set
    // This is where it usually hangs. We check for Errors too.
    while (1) {
        uint8_t status = inb(0x1F7);
        if (!(status & 0x80) && (status & 0x08)) break; // Not Busy and Data Ready
        if (status & 0x01) {
            kprintf_unsync("IDE Error during write!\n");
            return;
        }
    }

    // 3. Send data
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outw(0x1F0, ptr[i]);
    }

    // 4. Flush the write (Important for some emulators)
    outb(0x1F7, 0xE7); // Cache Flush command
    while (inb(0x1F7) & 0x80);
}


void fat_mkdir(const char* dirname) {
    uint8_t* dir_buf = (uint8_t*)kmalloc(512);
    uint8_t* new_dir_sector = (uint8_t*)kmalloc(512);

    if (!dir_buf || !new_dir_sector) {
        kprintf_unsync("MKDIR Error: Out of memory\n");
        if (dir_buf) kfree(dir_buf); // Clean up if only one failed
        return;
    }
    // 2. Find a free cluster for the new directory's contents
    uint16_t new_cluster = fat_find_free_cluster();
    if (new_cluster == 0xFFFF) {
        kprintf_unsync("MKDIR Error: Disk Full\n");
        goto cleanup;
    }

    // 3. Update FAT Table: Mark new cluster as End-of-Chain (0xFFFF)
    fat_update_table(new_cluster, 0xFFFF);

    // 4. Find an empty slot in the PARENT directory
    uint32_t parent_dir_lba = get_current_dir_lba();
    ide_read_sector(parent_dir_lba, dir_buf);
    struct fat_dir_entry* entries = (struct fat_dir_entry*)dir_buf;

    int slot = -1;
    for (int i = 0; i < 16; i++) {
        // 0x00 = Never used, 0xE5 = Deleted
        if (entries[i].name[0] == 0x00 || (unsigned char)entries[i].name[0] == 0xE5) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        kprintf_unsync("MKDIR Error: Parent directory is full\n");
        goto cleanup;
    }

    // 5. Create the entry in the Parent Directory
    kmemset(&entries[slot], 0, sizeof(struct fat_dir_entry));
    
    for (int i = 0; i < 8; i++) entries[slot].name[i] = ' ';
for (int i = 0; i < 3; i++) entries[slot].ext[i] = ' '; // Explicitly clear extension

for (int i = 0; i < 8 && dirname[i] != '\0'; i++) {
    char c = dirname[i];
    if (c >= 'a' && c <= 'z') c -= 32; 
    entries[slot].name[i] = c;
}

    entries[slot].attr = 0x10; // Directory Attribute
    entries[slot].first_cluster_low = new_cluster;
    entries[slot].size = 0;    // Directories report 0 size in FAT16

    // Write parent directory back to disk
    ide_write_sector(parent_dir_lba, dir_buf);

    // 6. Initialize the NEW directory's cluster with "." and ".."
    kmemset(new_dir_sector, 0, 512);
    struct fat_dir_entry* dot_entries = (struct fat_dir_entry*)new_dir_sector;

    // Create "." (Self)
    kmemcpy(dot_entries[0].name, ".       ", 8);
    dot_entries[0].attr = 0x10;
    dot_entries[0].first_cluster_low = new_cluster;

    // Create ".." (Parent)
    kmemcpy(dot_entries[1].name, "..      ", 8);
    dot_entries[1].attr = 0x10;
    // Current cluster is the parent of the one we just made
    dot_entries[1].first_cluster_low = (uint16_t)fat_get_current_cluster();

    // Write the new directory structure to its cluster on disk
    ide_write_sector(cluster_to_lba(new_cluster), new_dir_sector);

    kprintf_unsync("Directory '%s' created at cluster %d\n", dirname, new_cluster);

cleanup:
    kfree(dir_buf);
    kfree(new_dir_sector);
}
void fat_touch(const char* filename) {
    uint8_t* dir_buf = (uint8_t*)kmalloc(512);
    if (!dir_buf) return;

    // 1. Find an empty slot in the current directory
    uint32_t dir_lba = get_current_dir_lba();
    ide_read_sector(dir_lba, dir_buf);
    struct fat_dir_entry* entries = (struct fat_dir_entry*)dir_buf;

    int slot = -1;
    for (int i = 0; i < 16; i++) {
        if (entries[i].name[0] == 0x00 || (unsigned char)entries[i].name[0] == 0xE5) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        kprintf_unsync("TOUCH Error: Directory full\n");
        kfree(dir_buf);
        return;
    }

    // 2. Clear the entry
    kmemset(&entries[slot], 0, sizeof(struct fat_dir_entry));
    for (int i = 0; i < 8; i++) entries[slot].name[i] = ' ';
    for (int i = 0; i < 3; i++) entries[slot].ext[i] = ' ';

    // 3. Parse Filename (e.g., "test.txt")
    int dot_pos = -1;
    for (int i = 0; filename[i] != '\0'; i++) {
        if (filename[i] == '.') {
            dot_pos = i;
            break;
        }
    }

    // Copy Name part
    int name_len = (dot_pos == -1) ? kstrlen(filename) : dot_pos;
    for (int i = 0; i < name_len && i < 8; i++) {
        char c = filename[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        entries[slot].name[i] = c;
    }

    // Copy Extension part
    if (dot_pos != -1) {
        for (int i = 0; i < 3 && filename[dot_pos + 1 + i] != '\0'; i++) {
            char c = filename[dot_pos + 1 + i];
            if (c >= 'a' && c <= 'z') c -= 32;
            entries[slot].ext[i] = c;
        }
    }

    // 4. Set Attributes
    entries[slot].attr = 0x00; // Normal File
    entries[slot].first_cluster_low = 0; // 0 bytes, so no cluster assigned yet
    entries[slot].size = 0;

    // 5. Write back to disk
    ide_write_sector(dir_lba, dir_buf);
    kprintf_unsync("Created file: %s\n", filename);

    kfree(dir_buf);
}
void fat_hexdump_file(const char* filename) {
    // 1. Find the file
    struct fat_dir_entry* entry = fat_search(filename);
    
    if (!entry) {
        kprintf_unsync("HEXDUMP Error: File '%s' not found.\n", filename);
        return;
    }

    if (entry->attr & 0x10) {
        kprintf_unsync("HEXDUMP Error: '%s' is a directory.\n", filename);
        return;
    }

    if (entry->size == 0) {
        kprintf_unsync("File '%s' is empty (0 bytes).\n", filename);
        return;
    }

    // 2. Load the file into RAM
    void* data = fat_load_file(entry);
    
    if (data) {
        kprintf_unsync("Hexdump of %s (%d bytes):\n", filename, entry->size);
        
        // 3. Call your existing hexdump function
        hexdump(data, entry->size);
        
        // 4. Clean up memory!
        kfree(data);
    } else {
        kprintf_unsync("HEXDUMP Error: Failed to load file.\n");
    }
}
void fat_write_file(const char* filename, const char* data) {
    uint8_t* sector_buffer = (uint8_t*)kmalloc(512);
    uint32_t dir_lba = get_current_dir_lba();
    
    // 1. Find the file's directory entry
    ide_read_sector(dir_lba, sector_buffer);
    struct fat_dir_entry* entries = (struct fat_dir_entry*)sector_buffer;
    int slot = -1;

    for (int i = 0; i < 16; i++) {
        if (fat_compare_name(filename, (char*)entries[i].name, (char*)entries[i].ext)) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        kprintf("WRITE Error: File not found.\n");
        kfree(sector_buffer);
        return;
    }

    // --- SAFETY CHECK: ONLY 0-BYTE FILES ---
    if (entries[slot].size > 0 || entries[slot].first_cluster_low != 0) {
        kprintf("WRITE Error: File is not empty. This version only supports new files.\n");
        kfree(sector_buffer);
        return;
    }

    // 2. Allocation: Find a free cluster for this new file
    uint16_t cluster = fat_find_free_cluster();
    if (cluster == 0xFFFF) {
        kprintf("WRITE Error: Disk Full.\n");
        kfree(sector_buffer);
        return;
    }

    // 3. Update FAT Table: Mark cluster as End-of-Chain (0xFFFF)
    fat_update_table(cluster, 0xFFFF);

    // 4. Update Directory Entry: Set cluster and size
    uint32_t data_len = kstrlen(data);
    if (data_len > 511) data_len = 511; // Cap at one sector for now

    entries[slot].first_cluster_low = cluster;
    entries[slot].size = data_len;
    
    // Save updated directory back to disk
    ide_write_sector(dir_lba, sector_buffer);

    // 5. Write the actual data to the new cluster
    kmemset(sector_buffer, 0, 512);
    kmemcpy(sector_buffer, data, data_len);
    ide_write_sector(cluster_to_lba(cluster), sector_buffer);

    kprintf("Successfully wrote %d bytes to %s\n", data_len, filename);

    kfree(sector_buffer);
}
