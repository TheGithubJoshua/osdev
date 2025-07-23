// modified from https://github.com/bztsrc/raspi3-tutorial/blob/master/0D_readfile/main.c

#include "../ahci/ahci.h"
#include "../../timer.h"
#include "../../mm/pmm.h"
#include "../../memory.h"
#include "../../fs/fs.h"
//#include "../../util/string.h"
#include "fat.h"
#include "../../iodebug.h"

// get the end of bss segment from linker
extern unsigned char _end;

static unsigned int partitionlba = 0;

// the BIOS Parameter Block (in Volume Boot Record)
typedef struct {
    char            jmp[3];
    char            oem[8];
    unsigned char   bps0;
    unsigned char   bps1;
    unsigned char   spc;
    unsigned short  rsc;
    unsigned char   nf;
    unsigned char   nr0;
    unsigned char   nr1;
    unsigned short  ts16;
    unsigned char   media;
    unsigned short  spf16;
    unsigned short  spt;
    unsigned short  nh;
    unsigned int    hs;
    unsigned int    ts32;
    unsigned int    spf32;
    unsigned int    flg;
    unsigned int    rc;
    char            vol[6];
    char            fst[8];
    char            dmy[20];
    char            fst2[8];
} __attribute__((packed)) bpb_t;

// directory entry structure
/*typedef struct {
    char            name[8];
    char            ext[3];
    char            attr[9];
    unsigned short  ch;
    unsigned int    attr2;
    unsigned short  cl;
    unsigned int    size;
} __attribute__((packed)) fatdir_t;
I forgor i already had one*/

typedef struct {
    char name[13];         // 8.3 filename (null-terminated)
    unsigned int cluster;
} DirLookupEntry;

DirLookupEntry dir_lookup[MAX_DIR_LOOKUP];
int dir_lookup_count = 0;

static inline int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

static unsigned char fat_workbuf[FAT_WORKBUF_SIZE];

static bpb_t *bpb;
fat_dir_entry_t *dir_entry;

/**
 * Get the starting LBA address of the first partition
 * so that we know where our FAT file system starts, and
 * read that volume's BIOS Parameter Block
 */
int fat_getpartition(void)
{
    unsigned char *mbr = (unsigned char *)palloc(1, true);  // one sector (512 bytes)
    if (!mbr) {
        serial_puts("ERROR: Allocation failed\n");
        return 0;
    }

    if (!ahci_readblock(0, mbr, 1)) {
        serial_puts("ERROR: Cannot read MBR\n");
        return 0;
    }

    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        serial_puts("ERROR: Bad magic in MBR\n");
        return 0;
    }

    if (mbr[0x1C2] != 0xE && mbr[0x1C2] != 0xC) {
        serial_puts("ERROR: Wrong partition type\n");
        return 0;
    }

    partitionlba = mbr[0x1C6] |
                   (mbr[0x1C7] << 8) |
                   (mbr[0x1C8] << 16) |
                   (mbr[0x1C9] << 24);

    // reuse mbr buffer for VBR
    if (!ahci_readblock(partitionlba, mbr, 1)) {
        serial_puts("ERROR: Cannot read VBR\n");
        return 0;
    }

    if (!ahci_readblock(partitionlba, fat_workbuf, 1)) {
        serial_puts("ERROR: Cannot read VBR into fat_workbuf\n");
        return 0;
    }


    bpb = (bpb_t *)mbr;

    // Debug statements to print BPB values
    serial_puts("BPB Values:\n");
    serial_puts("spf16: ");
    serial_puthex(bpb->spf16);
    serial_puts("\n");
    serial_puts("spf32: ");
    serial_puthex(bpb->spf32);
    serial_puts("\n");
    serial_puts("nf: ");
    serial_puthex(bpb->nf);
    serial_puts("\n");
    serial_puts("rsc: ");
    serial_puthex(bpb->rsc);
    serial_puts("\n");
    serial_puts("nr0: ");
    serial_puthex(bpb->nr0);
    serial_puts("\n");
    serial_puts("nr1: ");
    serial_puthex(bpb->nr1);
    serial_puts("\n");
    serial_puts("spc: ");
    serial_puthex(bpb->spc);
    serial_puts("\n");
    serial_puts("partitionlba: ");
    serial_puthex(partitionlba);
    serial_puts("\n");
    serial_puts("rc (Root Cluster): ");
    serial_puthex(bpb->rc);
    serial_puts("\n");

    if (!(bpb->fst[0] == 'F' && bpb->fst[1] == 'A' && bpb->fst[2] == 'T') &&
        !(bpb->fst2[0] == 'F' && bpb->fst2[1] == 'A' && bpb->fst2[2] == 'T')) {
        serial_puts("ERROR: Unknown filesystem type\n");
        return 0;
    }

    unsigned int *fat32 = (unsigned int*)(fat_workbuf + 512); // FAT starts after 1 sector
    unsigned short *fat16 = (unsigned short*)fat32;
    return 1;
}

unsigned int fat_read_entry(unsigned int cluster) {
    unsigned int fat_offset, fat_sector, ent_offset;
    unsigned char FAT_table[512];  // Assuming 512 bytes per sector

    if (bpb->spf16) {
        // FAT16 entry is 2 bytes each
        fat_offset = cluster * 2;
    } else {
        // FAT32 entry is 4 bytes each
        fat_offset = cluster * 4;
    }

    fat_sector = partitionlba + bpb->rsc + (fat_offset / 512);
    ent_offset = fat_offset % 512;

    // Read the sector containing the FAT entry
    if (!ahci_readblock(fat_sector, FAT_table, 1)) {
        serial_puts("ERROR reading FAT sector\n");
        return 0xFFFFFFFF; // Error sentinel
    }

    unsigned int entry_value;

    if (bpb->spf16) {
        // FAT16: read 2 bytes as unsigned short
        unsigned short *fat16 = (unsigned short *)FAT_table;
        entry_value = fat16[ent_offset / 2];
    } else {
        // FAT32: read 4 bytes as unsigned int, mask upper 4 bits
        unsigned int *fat32 = (unsigned int *)FAT_table;
        entry_value = fat32[ent_offset / 4] & 0x0FFFFFFF;
    }

    return entry_value;
}

/**
 * Read a file into memory
 */
char *fat_readfile(unsigned int cluster, uint32_t *size)
{
    *size = dir_entry->size;

    serial_puts("\n(bpb->spf16 ? bpb->spf16 : bpb->spf32)");
    serial_puthex((bpb->spf16 ? bpb->spf16 : bpb->spf32));

    ahci_readblock(partitionlba + bpb->rsc, fat_workbuf + 512, (bpb->spf16 ? bpb->spf16 : bpb->spf32));
    // static probably not good
    unsigned char *t = palloc((FAT_WORKBUF_SIZE + PAGE_SIZE - 1) / PAGE_SIZE, true);

    ahci_readblock(partitionlba + bpb->rsc, t + 512, (bpb->spf16 ? bpb->spf16 : bpb->spf32));

    serial_puts("\n");
    unsigned int *fat32 = (unsigned int*)(t + 512); // FAT starts after 1 sector
    unsigned short *fat16 = (unsigned short*)fat32;
    unsigned int data_sec;
    unsigned char *data = t + 32 * 1024;  // Half for FAT, half for file
    unsigned char *ptr = data;

    // Compute where the data section begins
    data_sec = partitionlba + bpb->rsc + (bpb->nf * (bpb->spf16 ? bpb->spf16 : bpb->spf32));

    // Load the FAT
    serial_puts("\nbpb->spc: ");
    serial_puthex(bpb->spc);

    serial_puts("\nfatworkbuf: ");
    for (int i = 0; i < 16; i++) {
        serial_puthex(((uint8_t *)fat_workbuf)[i]);
        serial_puts(" ");
    }
    serial_puts("\n");

    serial_puts("partitionlba = ");
    serial_puthex(partitionlba);
    serial_puts("\n");

    serial_puts("bpb->rsc = ");
    serial_puthex(bpb->rsc);
    serial_puts("\n");

    serial_puts("FAT starts at LBA: ");
    serial_puthex(partitionlba + bpb->rsc);
    serial_puts("\n");

    unsigned int fat_sectors = bpb->spf16 ? bpb->spf16 : bpb->spf32;
    serial_puts("FAT sectors: ");
    serial_puthex(fat_sectors);
    serial_puts("\n");


    serial_puts("\nFAT count: ");
    serial_puthex(bpb->nf);           // Should be ≥1
    serial_puts("\nSectors per FAT: ");
    unsigned int spf = bpb->spf16 ? bpb->spf16 : bpb->spf32;
    serial_puthex(spf);              // Should be ≥1

    // Check hidden sectors (offset 0x1C-0x1F)
    unsigned hidden_sec = *(unsigned*)(t + 0x1C);
    serial_puts("Hidden sectors: ");
    serial_puthex(hidden_sec);  // Should match partition offset

    serial_puts("Raw FAT (16 bytes): ");
    for (int i = 0; i < 16; i++) {
        serial_puthex(t[512 + i]);
        serial_puts(" ");
    }
    serial_puts("\n");

    for (int i = 0; i < 16; i++) {
        serial_puts("FAT[");
        serial_puthex(i);
        serial_puts("] = ");
        serial_puthex((bpb->spf16 ? fat16[i] : fat32[i]));
        serial_puts("\n");
    }

    serial_puts("FAT16 entries:\n");
    for (int i = 0; i < 10; i++) {
        serial_puthex(fat16[i]);
        serial_puts(" ");
    }
    serial_puts("\n");

    serial_puts("FAT32 entries:\n");
    for (int i = 0; i < 10; i++) {
        serial_puthex(fat32[i]);
        serial_puts(" ");
    }
    serial_puts("\n");

    serial_puts("bps0: ");
    serial_puthex(bpb->bps0);
    serial_puts("\nbps1: ");
    serial_puthex(bpb->bps1);

    // Traverse cluster chain and load file content
   while (cluster > 1 && cluster < (bpb->spf16 ? 0xFFF8 : 0x0FFFFFF8)) {
    unsigned int lba = data_sec + (cluster - 2) * bpb->spc;
    ahci_readblock(lba, ptr, bpb->spc);

    ptr += bpb->spc * (bpb->bps0 + (bpb->bps1 << 8));

    // Debug print before reading FAT entry
    serial_puts("Current cluster: ");
    serial_puthex(cluster);
    serial_puts("\n");

        unsigned int next_cluster = fat_read_entry(cluster);

    serial_puts("Next cluster from FAT: ");
    serial_puthex(next_cluster);
    serial_puts("\n");

    // Sanity check:
    if (next_cluster == 0) {
        serial_puts("WARNING: FAT entry is zero — possible free cluster or corruption.\n");
        break;  // Or handle error
    }

    cluster = next_cluster;
} 
    return (char*)data;
}

/**
 * Convert a filename to the 8.3 format used by FAT file systems.
 * The output is stored in the provided `fn` array, which must be at least 11 bytes long.
 */
void convert_to_fat8_3(const char *input, char fn[11]) {
    // Initialize the output array with spaces
    memset(fn, ' ', 11);

    // Temporary buffers
    char base[9] = {0}; // 8 + null
    char ext[4]  = {0}; // 3 + null

    // Pointers to write into base/ext
    int base_pos = 0, ext_pos = 0;
    bool in_extension = false;

    // First pass: split input into base and extension
    while (*input) {
        if (*input == '.') {
            in_extension = true;
            input++;
            continue;
        }

        // Convert to uppercase
        char c = (*input >= 'a' && *input <= 'z') ? (*input - 'a' + 'A') : *input;

        // Replace invalid characters with '_'
        if (c == ' ' || c < 32 || c > 126) {
            c = '_';
        }

        if (!in_extension && base_pos < 255) { // allow long base for truncation
            base[base_pos++] = c;
        } else if (in_extension && ext_pos < 3) {
            ext[ext_pos++] = c;
        }

        input++;
    }

    base[base_pos] = '\0';
    ext[ext_pos] = '\0';

    // Handle base name truncation with ~1 if necessary
    if (base_pos > 8) {
        // Truncate to 6 characters and add "~1"
        memcpy(fn, base, 6);
        fn[6] = '~';
        fn[7] = '1';
    } else {
        memcpy(fn, base, base_pos);
    }

    // Add extension (already max 3 chars)
    memcpy(fn + 8, ext, ext_pos);
}

// Helper function to create a new list node
FileListNode* create_file_list_node(const char *filename) {
    FileListNode *node = (FileListNode *)palloc((sizeof(FileListNode) + PAGE_SIZE - 1) / PAGE_SIZE, true);
    if (!node) return NULL;
    strncpy(node->filename, filename, sizeof(node->filename) - 1);
    node->filename[sizeof(node->filename) - 1] = '\0'; // Ensure null-termination
    node->next = NULL;
    return node;
}

void fat_list_files(unsigned int cluster, FileListNode **file_list);

// Function to append a filename to the list
void append_to_file_list(FileListNode **head, const char *filename) {
    FileListNode *new_node = create_file_list_node(filename);
    if (!new_node) return; // Memory allocation failed

    if (*head == NULL) {
        *head = new_node;
    } else {
        FileListNode *current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }
}

unsigned int get_cluster_for_dir(const char *name) {
    for (int i = 0; i < dir_lookup_count; i++) {
        if (strcmp(name, dir_lookup[i].name) == 0) {
            return dir_lookup[i].cluster;
        }
    }
    return 0; // Not found
}

FileListNode* list_files(const char* name) {
        FileListNode *file_list = NULL;
        FileListNode *file_list2 = NULL;
        unsigned int root_cluster = bpb->rc;
        fat_list_files(root_cluster, &file_list2);
        unsigned int cluster = get_cluster_for_dir(name);  // Must match the 8.3 name format
        if (cluster) {
           fat_list_files(cluster, &file_list);
           return file_list;
    }
    return file_list;
    }

/**
 * List all files in the root directory and its subdirectories and return the list
 */
FileListNode* fat_list_all_files() {
    FileListNode *file_list = NULL;
    unsigned int root_cluster = bpb->rc;
    fat_list_files(root_cluster, &file_list);
    return file_list;
}

/**
 * Recursively list all files starting from a given cluster and append to the list
 */
void fat_list_files(unsigned int cluster, FileListNode **file_list) {
    if (!bpb) {
        serial_puts("ERROR: BPB not initialized\n");
        return;
    }

    unsigned char t[FAT_WORKBUF_SIZE];

    unsigned int first_data_sector = 
          partitionlba
        + bpb->rsc
        + (bpb->nf * bpb->spf32);

    unsigned int fat_lba = partitionlba + bpb->rsc;
    unsigned int fat_sectors = bpb->spf32;
    if (!ahci_readblock(fat_lba, t, fat_sectors)) {
        serial_puts("ERROR: Could not read FAT into memory\n");
        return;
    }

    unsigned int *fat32 = (unsigned int *)t;

    while (cluster < 0x0FFFFFF8) {
        unsigned int first_sector_of_cluster = 
              first_data_sector 
            + (cluster - 2) * bpb->spc;

        for (unsigned int sec_off = 0; sec_off < bpb->spc; sec_off++) {
            unsigned char sector_buf[512];
            if (!ahci_readblock(first_sector_of_cluster + sec_off, sector_buf, 1)) {
                serial_puts("ERROR: Failed to read directory sector\n");
                return;
            }

            for (int offset = 0; offset < 512; offset += 32) {
                unsigned char *entry = &sector_buf[offset];

                if (entry[0] == 0x00) {
                    break; // End of directory
                }
                if (entry[0] == 0xE5 || (entry[11] & 0x0F) == 0x0F) {
                    continue;
                }

                bool directory = false;
                char filename[13];
                if ((entry[11] & 0x10) == 0x10) {
                    directory = true;
                } else {
                    // Construct the file name
                    int c;
                    for (c = 0; c < 8 && entry[c] != ' '; c++) {
                        filename[c] = entry[c];
                    }
                    if (entry[8] != ' ') {
                        filename[c++] = '.';
                        for (int i = 8; i < 11 && entry[i] != ' '; i++) {
                            filename[c++] = entry[i];
                        }
                    }
                    filename[c] = '\0';
                }

                // If directory, descend inside it
                if (directory && !(entry[0] == '.' && (entry[1] == ' ' || entry[1] == '.'))) {
                    unsigned int high = ((unsigned int)entry[21] << 8) | entry[20];
                    unsigned int low  = ((unsigned int)entry[27] << 8) | entry[26];
                    unsigned int subdir_cluster = (high << 16) | low;

                    serial_puts("\nDescending into subdir cluster ");
                    serial_puthex(subdir_cluster);
                    serial_puts("\n");

                    // Build directory name
                    int c = 0;
                    for (c = 0; c < 8 && entry[c] != ' '; c++) {
                        filename[c] = entry[c];
                    }
                    if (entry[8] != ' ') {
                        filename[c++] = '.';
                        for (int i = 8; i < 11 && entry[i] != ' '; i++) {
                            filename[c++] = entry[i];
                        }
                    }
                    filename[c] = '\0';

                    append_to_file_list(file_list, filename); // Optional

                    // add to lookup array
                    if (dir_lookup_count < MAX_DIR_LOOKUP) {
                        strncpy(dir_lookup[dir_lookup_count].name, filename, 13);
                        dir_lookup[dir_lookup_count].cluster = subdir_cluster;
                        dir_lookup_count++;
                    } else {
                        serial_puts("WARNING: dir_lookup full!\n");
                    }

                    // Optional recursion
                    // fat_list_files(subdir_cluster, file_list);
                } else if (!directory) {
                    // Append filename to the list
                    append_to_file_list(file_list, filename);
                }

            }
        }

        unsigned int next = fat32[cluster] & 0x0FFFFFFF;
        if (next >= 0x0FFFFFF8) {
            break;
        }
        cluster = next;
    }
}

unsigned int fat_getcluster(char *fn, char parts[MAX_PARTS][MAX_LENGTH]) {
    if (!bpb) {
        serial_puts("ERROR: BPB not initialized\n");
        return 0;
    }

    unsigned char t[FAT_WORKBUF_SIZE];

    unsigned int first_data_sector = 
          partitionlba
        + bpb->rsc
        + (bpb->nf * bpb->spf32);

    serial_puts("FirstDataSector: "); serial_puthex(first_data_sector); serial_puts("\n");

    unsigned int root_cluster = bpb->rc;
    serial_puts("Root directory start cluster: "); serial_puthex(root_cluster); serial_puts("\n");

    unsigned int fat_lba = partitionlba + bpb->rsc;
    unsigned int fat_sectors = bpb->spf32;
    if (!ahci_readblock(fat_lba, t, fat_sectors)) {
        serial_puts("ERROR: Could not read FAT into memory\n");
        return 0;
    }

    unsigned int *fat32 = (unsigned int *)t;

    // Start with the root directory
    unsigned int current_cluster = root_cluster;
    for (int part_index = 0; parts[part_index][0] != '\0' && part_index < MAX_PARTS - 1; part_index++) {
        bool found_directory = false;
        
        while (current_cluster < 0x0FFFFFF8) {
            unsigned int first_sector_of_cluster = 
                  first_data_sector 
                + (current_cluster - 2) * bpb->spc;

            for (unsigned int sec_off = 0; sec_off < bpb->spc; sec_off++) {
                unsigned char sector_buf[512];
                if (!ahci_readblock(first_sector_of_cluster + sec_off, sector_buf, 1)) {
                    serial_puts("ERROR: Failed to read directory sector\n");
                    return E_FILE_READ_ERROR;
                }

                for (int offset = 0; offset < 512; offset += 32) {
                    unsigned char *entry = &sector_buf[offset];

                    if (entry[0] == 0x00) {
                        // End of this directory
                        break;
                    }
                    if (entry[0] == 0xE5) {
                        continue;
                    }
                    if ((entry[11] & 0x0F) == 0x0F) {
                        continue;
                    }

                    bool directory = false;
                    if ((entry[11] & 0x10) == 0x10) {
                        directory = true;
                    }

                    // Compare directory name
                    //char fn[11];
                    convert_to_fat8_3(parts[part_index], fn);
                    serial_puts("part_index: ");
                    serial_puthex(part_index);
                    serial_puts("fn69: ");
                    serial_puts(fn);
                    if (directory && !(entry[0] == '.' && (entry[1] == ' ' || entry[1] == '.')) && memcmp(entry + 0, fn, 11) == 0) {
                        unsigned int high = ((unsigned int)entry[21] << 8) | entry[20];
                        unsigned int low  = ((unsigned int)entry[27] << 8) | entry[26];
                        unsigned int subdir_cluster = (high << 16) | low;

                        serial_puts("Descending into subdir: ");
                        for (int c = 0; c < 8 && entry[c] != ' '; c++) serial_putc(entry[c]);
                        if (entry[8] != ' ') {
                            serial_putc('.');
                            for (int c = 8; c < 11 && entry[c] != ' '; c++) serial_putc(entry[c]);
                        }
                        serial_puts(" at cluster ");
                        serial_puthex(subdir_cluster);
                        serial_puts("\n");

                        current_cluster = subdir_cluster;
                        found_directory = true;
                        break;
                    }
                }

                if (found_directory) {
                    break;
                }
            }

            unsigned int next = fat32[current_cluster] & 0x0FFFFFFF;
            if (next >= 0x0FFFFFF8) {
                break;
            }
            current_cluster = next;
        }

        /*if (!found_directory) {
            // Directory not found
            serial_puts("ERROR: Directory not found\n");
            return 0;
        }
        */
    }

    // Now, the current cluster should be at the last directory level.
    while (current_cluster < 0x0FFFFFF8) {
        unsigned int first_sector_of_cluster = 
              first_data_sector 
            + (current_cluster - 2) * bpb->spc;

        for (unsigned int sec_off = 0; sec_off < bpb->spc; sec_off++) {
            unsigned char sector_buf[512];
            if (!ahci_readblock(first_sector_of_cluster + sec_off, sector_buf, 1)) {
                serial_puts("ERROR: Failed to read directory sector\n");
                return E_FILE_READ_ERROR;
            }

            for (int offset = 0; offset < 512; offset += 32) {
                unsigned char *entry = &sector_buf[offset];

                if (entry[0] == 0x00) {
                    // End of this directory
                    break;
                }
                if (entry[0] == 0xE5) {
                    continue;
                }
                if ((entry[11] & 0x0F) == 0x0F) {
                    continue;
                }

                // Compare filename
                if (memcmp(entry + 0, fn, 11) == 0) {
                    unsigned int high = ((unsigned int)entry[21] << 8) | entry[20];
                    unsigned int low  = ((unsigned int)entry[27] << 8) | entry[26];
                    unsigned int full_cluster = (high << 16) | low;

                    dir_entry = (fat_dir_entry_t*)&sector_buf[offset];
                    serial_puts("fat file size: ");
                    serial_puthex(dir_entry->size);

                    serial_puts("Found file2: ");
                    for (int i = 0; i < 8 && entry[i] != ' '; i++) serial_putc(entry[i]);
                    if (entry[8] != ' ') {
                        serial_putc('.');
                        for (int i = 8; i < 11 && entry[i] != ' '; i++) serial_putc(entry[i]);
                    }
                    serial_puts(" at cluster ");
                    serial_puthex(full_cluster);
                    serial_puts("\n");

                    return full_cluster;
                }
            }
        }

        unsigned int next = fat32[current_cluster] & 0x0FFFFFFF;
        if (next >= 0x0FFFFFF8) {
            break;
        }
        current_cluster = next;
    }

    // File not found in the specified path
    serial_puts("ERROR2: file not found\n");
    return E_FILE_NOT_FOUND;
}

uint64_t split_string(const char *str, const char delimiter, char parts[MAX_PARTS][256]) {
    int part_count = 0;
    while (*str) {
        if (*str == delimiter) {
            str++;
            continue;
        }
        
        parts[part_count][0] = '\0'; // Clear the string buffer
        int i = 0;
        while (*str && *str != delimiter && i < 255) { // Ensure we don't overflow the buffer
            parts[part_count][i++] = *str++;
        }
        parts[part_count++][i] = '\0';
    }
    return part_count;
}


char* fat_read(const char input[MAX_LENGTH], uint32_t file_size) {
    // parse path
    char parts[MAX_PARTS][256];
    uint64_t part_count = split_string(input, '/', parts);
    serial_puts("parts: ");
    serial_puts((const char*)parts[part_count - 1]);

    char *fn;
    // read
    unsigned int cluster = fat_getcluster(fn, parts);
    if (cluster) {
        char *filedata = fat_readfile(cluster, &file_size);
        if (filedata) {
            serial_puts("file from subdir successfully read!");
            return filedata;
        } else {
            serial_puts("ERROR reading file from subdir into memory\n");
        }
    } else if (!cluster) {
        serial_puts("no from subdir cluster\n");
    } else {
        serial_puts("FAT partition not found???\n");
    }
    return NULL;
}

int fat_get_dir_entry(const char *filename, char parts[MAX_PARTS][256], fat_dir_entry_t *dir_entry) {
    if (!bpb || !filename || !dir_entry) {
        serial_puts("invalid args!");
        return E_INVALID_ARG;
    }

    char fn[11];
    convert_to_fat8_3((char*)filename, fn);
    
    unsigned char t[FAT_WORKBUF_SIZE];
    unsigned int first_data_sector = partitionlba + bpb->rsc + (bpb->nf * bpb->spf32);
    unsigned int root_cluster = bpb->rc;
    unsigned int fat_lba = partitionlba + bpb->rsc;
    unsigned int fat_sectors = bpb->spf32;
    
    if (!ahci_readblock(fat_lba, t, fat_sectors)) {
        return E_FILE_READ_ERROR;
    }
    unsigned int *fat32 = (unsigned int *)t;

    // Navigate through directory path
    unsigned int current_cluster = root_cluster;
    for (int part_index = 0; parts[part_index][0] != '\0' && part_index < MAX_PARTS - 1; part_index++) {
        bool found_directory = false;
        
        while (current_cluster < 0x0FFFFFF8) {
            unsigned int first_sector_of_cluster = first_data_sector + (current_cluster - 2) * bpb->spc;

            for (unsigned int sec_off = 0; sec_off < bpb->spc; sec_off++) {
                unsigned char sector_buf[512];
                if (!ahci_readblock(first_sector_of_cluster + sec_off, sector_buf, 1)) {
                    return E_FILE_READ_ERROR;
                }

                for (int offset = 0; offset < 512; offset += 32) {
                    unsigned char *entry = &sector_buf[offset];

                    if (entry[0] == 0x00) break;
                    if (entry[0] == 0xE5) continue;
                    if ((entry[11] & 0x0F) == 0x0F) continue;

                    bool directory = (entry[11] & 0x10) == 0x10;
                    char part_fn[11];
                    convert_to_fat8_3(parts[part_index], part_fn);

                    if (directory && !(entry[0] == '.' && (entry[1] == ' ' || entry[1] == '.')) 
                        && memcmp(entry, part_fn, 11) == 0) {
                        unsigned int high = ((unsigned int)entry[21] << 8) | entry[20];
                        unsigned int low = ((unsigned int)entry[27] << 8) | entry[26];
                        current_cluster = (high << 16) | low;
                        found_directory = true;
                        break;
                    }
                }
                if (found_directory) break;
            }

            if (found_directory) break;
            
            unsigned int next = fat32[current_cluster] & 0x0FFFFFFF;
            if (next >= 0x0FFFFFF8) break;
            current_cluster = next;
        }

        if (!found_directory) {
            return E_FILE_NOT_FOUND;
        }
    }

    // Search for file in final directory
    while (current_cluster < 0x0FFFFFF8) {
        unsigned int first_sector_of_cluster = first_data_sector + (current_cluster - 2) * bpb->spc;
        
        for (unsigned int sec_off = 0; sec_off < bpb->spc; sec_off++) {
            unsigned char sector_buf[512];
            if (!ahci_readblock(first_sector_of_cluster + sec_off, sector_buf, 1)) {
                return E_FILE_READ_ERROR;
            }

            for (int offset = 0; offset < 512; offset += 32) {
                unsigned char *entry = &sector_buf[offset];
                
                if (entry[0] == 0x00) break;
                if (entry[0] == 0xE5) continue;
                if ((entry[11] & 0x0F) == 0x0F) continue;

                if (memcmp(entry, fn, 11) == 0) {
                    // Copy directory entry data
                    serial_puts("size (from get_dir_entry): ");
                    serial_puthex(dir_entry->size);
                    memcpy(dir_entry, entry, sizeof(fat_dir_entry_t));
                    return FILE_SUCCESS;
                }
            }
        }

        unsigned int next = fat32[current_cluster] & 0x0FFFFFFF;
        if (next >= 0x0FFFFFF8) break;
        current_cluster = next;
    }
    return E_FILE_NOT_FOUND;
}