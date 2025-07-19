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
#include <stdint.h>

#define FAT_WORKBUF_SIZE  512 * 1024  // 512 KB buffer to hold the entire FAT
#define MAX_PARTS 100
#define MAX_LENGTH 256

// Define a node for the linked list
typedef struct file_list_node {
    char filename[13]; // 8 characters + 3 characters + null terminator
    struct file_list_node *next;
} FileListNode;

int fat_getpartition(void);
unsigned int fat_getcluster(char *fn, char parts[MAX_PARTS][MAX_LENGTH]);
char *fat_readfile(unsigned int cluster);
void convert_to_fat8_3(const char *input, char fn[11]);
unsigned int fat_getcluster_recursive(char *path);
FileListNode* fat_list_all_files();
char* fat_read(const char input[MAX_LENGTH]);
static inline uint64_t split_string(const char *str, const char delimiter, char parts[MAX_PARTS][256]);