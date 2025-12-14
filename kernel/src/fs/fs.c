/* totally useful and posix compliant VFS fr */
#include "fs.h"
#include "../iodebug.h"
#include "../mm/pmm.h"
#include "../errno.h"
#include <stdint.h>

BYTE posix_to_fatfs_mode(int flags) {
    BYTE mode = 0;

    // Access mode
    switch (flags & 0x3) { // mask O_RDONLY, O_WRONLY, O_RDWR
        case O_RDONLY:
            mode |= FA_READ;
            break;
        case O_WRONLY:
            mode |= FA_WRITE;
            break;
        case O_RDWR:
            mode |= FA_READ | FA_WRITE;
            break;
    }

    // Creation flags
    if (flags & O_CREAT) {
        if (flags & O_EXCL) {
            mode |= FA_CREATE_NEW;      // fail if exists
        } else if (flags & O_TRUNC) {
            mode |= FA_CREATE_ALWAYS;   // truncate existing
        } else {
            mode |= FA_OPEN_ALWAYS;     // open or create
        }
    } else {
        mode |= FA_OPEN_EXISTING;       // default: open existing
    }

    // Append mode
    if (flags & O_APPEND) {
        mode &= ~(FA_CREATE_ALWAYS | FA_CREATE_NEW); // override creation
        mode |= FA_OPEN_APPEND | FA_WRITE;
    }

    return mode;
}

static const int fatfs_errno_map[] = {
    [FR_OK]                   = 0,
    [FR_DISK_ERR]             = EIO,
    [FR_INT_ERR]              = EIO,
    [FR_NOT_READY]            = ENODEV,
    [FR_NO_FILE]              = ENOENT,
    [FR_NO_PATH]              = ENOENT,
    [FR_INVALID_NAME]         = EINVAL,
    [FR_DENIED]               = EACCES,
    [FR_EXIST]                = EEXIST,
    [FR_INVALID_OBJECT]       = EBADF,
    [FR_WRITE_PROTECTED]      = EROFS,
    [FR_INVALID_DRIVE]        = ENODEV,
    [FR_NOT_ENABLED]          = ENXIO,
    [FR_NO_FILESYSTEM]        = ENODEV,
    [FR_MKFS_ABORTED]         = ECANCELED,
    [FR_TIMEOUT]              = ETIMEDOUT,
    [FR_LOCKED]               = EBUSY,
    [FR_NOT_ENOUGH_CORE]      = ENOMEM,
    [FR_TOO_MANY_OPEN_FILES]  = EMFILE,
    [FR_INVALID_PARAMETER]    = EINVAL,
};

fd_t* file_descriptors[MAX_FILES];

int allocate_fd(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        // Check if slot is empty or not open
        if (file_descriptors[i] == NULL || !file_descriptors[i]->is_open) {

            // Allocate only if it doesn't already exist
            if (file_descriptors[i] == NULL) {
                file_descriptors[i] = (void*)palloc((sizeof(fd_t) + PAGE_SIZE - 1) / PAGE_SIZE, true);
                if (!file_descriptors[i])
                    return ENOMEM;
            }

            fd_t *fd = file_descriptors[i];
            fd->is_open = 1;
            fd->device = 0;
            fd->fs = 0;
            fd->pos = 0;
            fd->size = 0;
            fd->offset = 0;
            fd->access = 0;

            return i;
        }
    }

    return ENFILE;
}


void release_fd(int fd) {
    if (fd >= 0 && fd < MAX_FILES) {
        file_descriptors[fd]->is_open = 0;
        file_descriptors[fd]->device = 0;
        file_descriptors[fd]->fs = 0;
        file_descriptors[fd]->pos = 0;
        file_descriptors[fd]->size = 0;
        file_descriptors[fd]->access = 0;
    }
}

FIL *f;
FRESULT fr;
FATFS *FatFs;
static int fs_mounted = 0;

int open(const char *path, int flags, mode_t mode) {
    if (!fs_mounted) {
    FatFs = (void*)palloc((sizeof(FATFS) + PAGE_SIZE - 1) / PAGE_SIZE, true);
    FRESULT m = f_mount(FatFs, "", 1);
    serial_puts("f_mount result(2): ");
    serial_puthex(m);
    fs_mounted = 1;
}
    f = (void*)palloc((sizeof(FIL) + PAGE_SIZE - 1) / PAGE_SIZE, true);

    if (!path) {
        return -EINVAL;
    }
    
    int fd = allocate_fd();
    if (fd < 0) {
        return -fd; // Return error code
    }

    file_descriptors[fd]->device = DEVICE_AHCI;
    file_descriptors[fd]->fs = FS_FAT;
    file_descriptors[fd]->access = mode;
    
    if (file_descriptors[fd]->fs == FS_FAT) {
        fr = f_open(f, path, posix_to_fatfs_mode(flags));
        serial_puts("incoming fd: ");
        serial_puthex(fd);
        serial_puts("incoming fr: ");
        serial_puthex(fr);
        if (fr != FR_OK) {
            return -fatfs_errno_map[fr];
        }

        file_descriptors[fd]->is_open = 1;
        file_descriptors[fd]->device  = f->obj.fs->pdrv;      /* or whatever identifies the drive */
        file_descriptors[fd]->size    = f->obj.objsize;
        file_descriptors[fd]->pos     = f->fptr;
        file_descriptors[fd]->offset  = 0;                    /* or logical offset within FS */
        file_descriptors[fd]->access  = f->flag;              /* convert FatFs flags to mode_t bits */
        file_descriptors[fd]->private = f;
        
        return fd;
    }
    
    // Other filesystems not implemented
    release_fd(fd);
    return -ENOSYS;
}

unsigned int get_size(int fd) {
    return file_descriptors[fd]->size;
}

DIR* d;

int opendir(const char *path) {
    if (!fs_mounted) {
    FatFs = (void*)palloc((sizeof(FATFS) + PAGE_SIZE - 1) / PAGE_SIZE, true);
    FRESULT m = f_mount(FatFs, "", 1);
    serial_puts("f_mount result(2): ");
    serial_puthex(m);
    fs_mounted = 1;
}
    d = (void*)palloc((sizeof(DIR) + PAGE_SIZE - 1) / PAGE_SIZE, true);

    if (!path) {
        return E_INVALID_ARG;
    }
    
    int fd = allocate_fd();
    if (fd < 0) {
        return fd; // Return error code
    }
    
    file_descriptors[fd]->device = DEVICE_AHCI;
    file_descriptors[fd]->fs = FS_FAT;
    
    if (file_descriptors[fd]->fs == FS_FAT) {
        fr = f_opendir(d, path);
        serial_puts("incoming fd: ");
        serial_puthex(fd);
        serial_puts("incoming fr: ");
        serial_puthex(fr);
        if (fr != FR_OK) {
            return -fatfs_errno_map[fr];
        }

        file_descriptors[fd]->is_open = 1;
        file_descriptors[fd]->device  = d->obj.fs->pdrv;      /* or whatever identifies the drive */
        file_descriptors[fd]->size    = d->obj.objsize;
        file_descriptors[fd]->pos     = d->dptr;
        file_descriptors[fd]->offset  = 0;                    /* or logical offset within FS */
        file_descriptors[fd]->private = d;
        
        return fd;
    }
    
    // Other filesystems not implemented
    release_fd(fd);
    return E_NOT_IMPLEMENTED;
}

int readdir(int fd, FILINFO* fno) {
    if (fd < 0 || fd >= MAX_FILES || !file_descriptors[fd]->is_open) {
        return -EINVAL;
    }
    
    if (file_descriptors[fd]->fs == FS_FAT) {
        serial_puts("@@fsisfat");

        DIR *d = (DIR *)file_descriptors[fd]->private;
        FRESULT fr = f_readdir(d, fno);
        if (fr != FR_OK) {
            return -fatfs_errno_map[fr];
        } else { return fr; }
    }
    
    return -ENOSYS;
}

int close(int fd) {
    if (fd < 0 || fd >= MAX_FILES || !file_descriptors[fd]->is_open) {
        return -EINVAL;
    }
    
    FIL *fil = (FIL *)file_descriptors[fd]->private;
    f_close(fil);
    release_fd(fd);
    pfree(FatFs, (sizeof(FATFS) + PAGE_SIZE - 1) / PAGE_SIZE);
    return FILE_SUCCESS;
}

int read(int fd, char *buf, size_t count) {
    if (fd < 0 || fd >= MAX_FILES || !file_descriptors[fd]->is_open) {
        return -EINVAL;
    }
    
    if (!buf || count == 0) {
        return -EINVAL;
    }
    
    if (file_descriptors[fd]->fs == FS_FAT) {
        serial_puts("@@fsisfat");

        FIL *fil = (FIL *)file_descriptors[fd]->private;
        uint32_t size = 0;
        UINT br;
        FRESULT fr = f_read(fil, buf, count, &br);
        if (fr != FR_OK) {
            return -fatfs_errno_map[fr];
        }

        return br;
    }
    
    return -ENOSYS;
}

int write(int fd, const char *buf, size_t count) {
    if (fd < 0 || fd >= MAX_FILES || !file_descriptors[fd]->is_open) {
        return -EINVAL;
    }
    
    if (!buf || count == 0) {
        return -EINVAL;
    }
    
    if (file_descriptors[fd]->fs == FS_FAT) {
        serial_puts("@@fsisfat");

        FIL *fil = (FIL *)file_descriptors[fd]->private;
        uint32_t size = 0;
        UINT bw;
        FRESULT fr = f_write(fil, buf, count, &bw);
        if (fr != FR_OK) {
            return -fatfs_errno_map[fr];
        }

        return bw;
    }
    
    return -ENOSYS;
}

off_t lseek(int fd, off_t offset, int whence) {
    if (fd < 0 || fd >= MAX_FILES || !file_descriptors[fd]->is_open) {
        return -EINVAL;
    }
    
    off_t new_pos;
    
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = file_descriptors[fd]->pos + offset;
            break;
        case SEEK_END:
            new_pos = file_descriptors[fd]->size + offset;
            break;
        default:
            return -EINVAL;
    }
    
    if (new_pos < 0) {
        return -EINVAL;
    }
    
    file_descriptors[fd]->pos = new_pos;
    return new_pos;
}

// sync all open files to disk
int sync(void) {
    int ret = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_descriptors[i] && file_descriptors[i]->is_open) {
            FIL *fil = file_descriptors[i]->private;
            FRESULT fr = f_sync(fil);
            if (fr != FR_OK) {
                return -fatfs_errno_map[fr];
            }
        }
    }
    return ret;
}