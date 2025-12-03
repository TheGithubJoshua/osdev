#pragma once

#define MAX_FILES 100
#define E_FILE_NOT_FOUND -1
#define E_FILE_READ_ERROR 2
#define FILE_SUCCESS 0
#define E_TOO_MANY_FILES 3

#define DEVICE_AHCI 1
#define DEVICE_UART 2
#define DEVICE_SCREEN 3

#define FS_FAT 1

#define ACCESS_READ 0
#define ACCESS_WRITE 1

typedef long long off_t;
typedef unsigned int mode_t;
typedef long time_t;

#define E_INVALID_ARG       -1
#define E_INVALID_FD        -2
#define E_INVALID_PATH      -3
#define E_FILE_WRITE_ERROR  -6
#define E_NOT_IMPLEMENTED   -8
#define FILE_SUCCESS        0

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

#define S_IFMT      0170000   // File type mask
#define S_IFREG     0100000   // Regular file
#define S_IFDIR     0040000   // Directory

#define FAT_ATTR_READONLY   0x01
#define FAT_ATTR_DIRECTORY  0x10

#include <stddef.h>
#include "../drivers/fat/fat.h"
#include "../ff16/source/ff.h"

typedef unsigned int mode_t;

typedef struct fd {
	int is_open;
	int device;
	int fs;
	int size;
	int pos;
	size_t offset;
	mode_t access;
    void *private;  // store FIL*
} fd_t;

typedef struct {
    mode_t st_mode;
    off_t st_size;
    long st_blocks;
    long st_blksize;
    int st_nlink;
    int st_uid;
    int st_gid;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
} stat_t;

int allocate_fd();
void release_fd(int fd);
int open(const char *path, int flags, mode_t mode);
int close(int fd);
int read(int fd, char *buf, size_t count);
int write(int fd, const char *buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);
int stat(const char *path, stat_t *buf);
int opendir(const char *path);
int readdir(int fd, FILINFO* fno);
unsigned int get_size(int fd);
int sync();