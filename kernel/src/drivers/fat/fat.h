/*
 * Copyright (C) 2018 bzt (bztsrc@github)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */
#pragma once
#include <stdint.h>

#define FAT_WORKBUF_SIZE  512 * 1024  // 512 KB buffer to hold the entire FAT
#define MAX_PARTS 100
#define MAX_LENGTH 256

/* File attributes */
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LONG_NAME  0x0F  /* Long filename entry */

#define MAX_DIR_LOOKUP 100

// Define a node for the linked list
typedef struct file_list_node {
    char filename[13]; // 8 characters + 3 characters + null terminator
    struct file_list_node *next;
} FileListNode;

typedef struct {
    uint8_t name[11];           // 0-10: filename
    uint8_t attr;               // 11: attributes (1 byte, not 9!)
    uint8_t reserved;           // 12: reserved
    uint8_t creation_time_hundredths; // 13
    uint16_t creation_time;     // 14-15
    uint16_t creation_date;     // 16-17
    uint16_t last_accessed_date; // 18-19
    uint16_t cluster_high;      // 20-21
    uint16_t last_modified_time; // 22-23
    uint16_t last_modified_date; // 24-25
    uint16_t cluster_low;       // 26-27
    uint32_t size;              // 28-31: file size
} __attribute__((packed)) fat_dir_entry_t;

int fat_getpartition(void);
unsigned int fat_getcluster(char *fn, char parts[MAX_PARTS][MAX_LENGTH]);
char *fat_readfile(unsigned int cluster, uint32_t *size);
void convert_to_fat8_3(const char *input, char fn[11]);
unsigned int fat_getcluster_recursive(char *path);
FileListNode* fat_list_all_files();
char* fat_read(const char input[MAX_LENGTH], uint32_t file_size);
uint64_t split_string(const char *str, const char delimiter, char parts[MAX_PARTS][256]);
void fat_list_files(unsigned int cluster, FileListNode **file_list);
unsigned int get_cluster_for_dir(const char *name);
FileListNode* list_files(const char* name);
int fat_get_dir_entry(const char *filename, char parts[MAX_PARTS][256], fat_dir_entry_t *dir_entry);