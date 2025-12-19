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

// POSIX flags
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_APPEND 0x0008
#define O_CREAT  0x0200
#define O_TRUNC  0x0400
#define O_EXCL   0x0800

#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2

#define TYPE_STDIN 2
#define TYPE_STDOUT 3
#define TYPE_STDERR 4

#define FATFS_TO_ERRNO(fr) \
    (((unsigned)(fr) < ARRAY_SIZE(fatfs_errno_map)) \
        ? fatfs_errno_map[(fr)] : EIO)

#define IS_LEAP(y) (((y) % 4 == 0) && (((y) % 100 != 0) || ((y) % 400 == 0))) // is a given year a leap year

#include <stddef.h>
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
    const char *path;
    void *private;  // store FIL*
} fd_t;

struct timespec {
    time_t  tv_sec;         /* seconds */
    long    tv_nsec;        /* and nanoseconds */
};

struct stat {
    uint64_t st_dev;      /* dev_t     */
    uint64_t st_ino;      /* ino_t     */
    uint32_t st_mode;     /* mode_t    */
    uint64_t st_nlink;    /* nlink_t   */
    uint32_t st_uid;      /* uid_t     */
    uint32_t st_gid;      /* gid_t     */
    uint64_t st_rdev;     /* dev_t     */
    int64_t  st_size;     /* off_t     */
    int64_t  st_blksize;  /* blksize_t */
    int64_t  st_blocks;   /* blkcnt_t  */
    struct timespec st_atim; /* Last data access timestamp. */
    struct timespec st_mtim; /* Last data modification timestamp. */
    struct timespec st_ctim; /* Last file status change timestamp. */
};

int allocate_fd();
void release_fd(int fd);
int open(const char *path, int flags, mode_t mode);
int close(int fd);
int read(int fd, char *buf, size_t count);
int write(int fd, const char *buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);
int fstat(int fd, struct stat *st);
int opendir(const char *path);
int readdir(int fd, FILINFO* fno);
unsigned int get_size(int fd);
int sync();