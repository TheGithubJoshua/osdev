/* totally useful and posix compliant VFS fr */
/*#include "fs.h"
#include "../iodebug.h"
#include "../errno.h"
#include <stdint.h>
fd_t file_descriptors[MAX_FILES];
int allocate_fd() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!file_descriptors[i].is_open) {
            file_descriptors[i].is_open = 1;
            return i;
        }
    }
    return E_TOO_MANY_FILES; // No available FD
}
void release_fd(int fd) {
    if (fd >= 0 && fd <= MAX_FILES) {
        file_descriptors[fd].is_open = 0;
    }
}
int open(char path, int flags, mode_t mode) {
    int fd = allocate_fd();
    file_descriptors[fd].device = DEVICE_AHCI; // assume AHCI for now
    file_descriptors[fd].fs = FS_FAT; // assume FAT for now
    file_descriptors[fd].access = mode;
    if(file_descriptors[fd].fs == FS_FAT) {
    // parse path
    char parts[MAX_PARTS][256];
    uint64_t part_count = split_string(path, '/', parts);
    serial_puts("parts: ");
    serial_puts((const char)parts[part_count - 1]);
    char *fn;
    // read
    unsigned int cluster = fat_getcluster(fn, parts);
    file_descriptors[fd].pos = cluster;
    // set size later
    return fd;
}
if (file_descriptors[fd].pos == E_FILE_READ_ERROR) {
    release_fd(fd);
    return E_FILE_READ_ERROR;
}
release_fd(fd);
return fd;
}
int close(int fd) {
    file_descriptors[fd].device = 0;
    file_descriptors[fd].fs = 0;
    file_descriptors[fd].access = 0;
    file_descriptors[fd].device = 0;
    file_descriptors[fd].pos = 0;
    file_descriptors[fd].size = 0;
    file_descriptors[fd].access = 0;
    release_fd(fd);
    return FILE_SUCCESS; // return success for now 
}
int read(int fd, char *buf, size_t count) {
    uint32_t size = 0;
    char *data;
    if (file_descriptors[fd].fs == FS_FAT) {
        serial_puts("@@fsisfat");
        if (file_descriptors[fd].pos == 0) {
            serial_puts("@@filenotfound");
            return E_FILE_NOT_FOUND;
        }
        data = fat_readfile(file_descriptors[fd].pos, size); // read cluster chain at pos
    }
    size_t i;
    serial_puts("da buffer: ");
    for (i = 0; i < count; i++) {
        serial_putc(data[i]); // print each byte in hex
        buf[i] = data[i]; // stream file into buf up to count
    }
    buf[count+1] = 0; // 0 to indicate end
    if (buf == NULL) {
        return E_FILE_READ_ERROR;
    }
    return i;
}
*/
/* VFS implementation with proper error handling and structure */
#include "fs.h"
#include "../iodebug.h"
#include "../drivers/fat/fat.h"
#include "../errno.h"
#include <stdint.h>

fd_t file_descriptors[MAX_FILES];

int allocate_fd() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!file_descriptors[i].is_open) {
            file_descriptors[i].is_open = 1;
            // Initialize fd structure
            file_descriptors[i].device = 0;
            file_descriptors[i].fs = 0;
            file_descriptors[i].pos = 0;
            file_descriptors[i].size = 0;
            file_descriptors[i].access = 0;
            return i;
        }
    }
    return E_TOO_MANY_FILES;
}

void release_fd(int fd) {
    if (fd >= 0 && fd < MAX_FILES) {
        file_descriptors[fd].is_open = 0;
        file_descriptors[fd].device = 0;
        file_descriptors[fd].fs = 0;
        file_descriptors[fd].pos = 0;
        file_descriptors[fd].size = 0;
        file_descriptors[fd].access = 0;
    }
}

int open(const char *path, int flags, mode_t mode) {
    if (!path) {
        return E_INVALID_ARG;
    }
    
    int fd = allocate_fd();
    if (fd < 0) {
        return fd; // Return error code
    }
    
    file_descriptors[fd].device = DEVICE_AHCI;
    file_descriptors[fd].fs = FS_FAT;
    file_descriptors[fd].access = mode;
    
    if (file_descriptors[fd].fs == FS_FAT) {
        // Parse path
        char parts[MAX_PARTS][256];
        uint64_t part_count = split_string((char*)path, '/', parts);
        
        if (part_count == 0) {
            release_fd(fd);
            return E_INVALID_PATH;
        }
        
        serial_puts("parts: ");
        serial_puts((const char*)parts[part_count - 1]);
        
        // Get file cluster
        unsigned int cluster = fat_getcluster(0, parts);
        
        if (cluster == 0 || cluster == (unsigned int)E_FILE_READ_ERROR) {
            release_fd(fd);
            return E_FILE_NOT_FOUND;
        }
        
        file_descriptors[fd].pos = cluster;
        // TODO: Set file size from FAT directory entry
        file_descriptors[fd].size = 0; // Placeholder
        
        return fd;
    }
    
    // Other filesystems not implemented
    release_fd(fd);
    return E_NOT_IMPLEMENTED;
}

int close(int fd) {
    if (fd < 0 || fd >= MAX_FILES || !file_descriptors[fd].is_open) {
        return E_INVALID_FD;
    }
    
    release_fd(fd);
    return FILE_SUCCESS;
}

int read(int fd, char *buf, size_t count) {
    if (fd < 0 || fd >= MAX_FILES || !file_descriptors[fd].is_open) {
        return E_INVALID_FD;
    }
    
    if (!buf || count == 0) {
        return E_INVALID_ARG;
    }
    
    if (file_descriptors[fd].fs == FS_FAT) {
        serial_puts("@@fsisfat");
        
        if (file_descriptors[fd].pos == 0) {
            serial_puts("@@filenotfound");
            return E_FILE_NOT_FOUND;
        }
        
        uint32_t size = 0;
        char *data = fat_readfile(file_descriptors[fd].pos, &size);

        if (!data) {
            return E_FILE_READ_ERROR;
        }
        
        size_t bytes_to_read = (size < count) ? (size_t)size : count;

        serial_puts("data buffer: ");
        for (size_t i = 0; i < bytes_to_read; i++) {
            serial_putc(data[i]);
            buf[i] = data[i];
        }
        
        // Don't null-terminate - that's application responsibility
        // buf[count+1] = 0; // REMOVED - buffer overrun and not VFS responsibility
        
        return bytes_to_read;
    }
    
    return E_NOT_IMPLEMENTED;
}

off_t lseek(int fd, off_t offset, int whence) {
    if (fd < 0 || fd >= MAX_FILES || !file_descriptors[fd].is_open) {
        return E_INVALID_FD;
    }
    
    off_t new_pos;
    
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = file_descriptors[fd].pos + offset;
            break;
        case SEEK_END:
            new_pos = file_descriptors[fd].size + offset;
            break;
        default:
            return E_INVALID_ARG;
    }
    
    if (new_pos < 0) {
        return E_INVALID_ARG;
    }
    
    file_descriptors[fd].pos = new_pos;
    return new_pos;
}

int stat(const char *path, stat_t *buf) { // TODO: fix
    if (!path || !buf) {
        return E_INVALID_ARG;
    }
    
    // Parse path to get file info
    char parts[MAX_PARTS][256];
    uint64_t part_count = split_string((char*)path, '/', parts);
    
    if (part_count == 0) {
        return E_INVALID_PATH;
    }
    
    // Get FAT directory entry
    fat_dir_entry_t dir_entry;
    fat_get_dir_entry(parts[part_count], parts, &dir_entry);
    
    // Convert FAT attributes to POSIX mode
    mode_t mode = 0;
    if (dir_entry.attr & FAT_ATTR_DIRECTORY) {
        mode = S_IFDIR | 0755;  // Directory
    } else {
        mode = S_IFREG | 0644;  // Regular file
    }
    if (dir_entry.attr & FAT_ATTR_READONLY) {
        mode &= ~0222;  // Remove write permissions
    }
    
    // Convert FAT timestamps to Unix time (implement later™)
    //time_t mtime = fat_time_to_unix(dir_entry.write_date, dir_entry.write_time);
    //time_t atime = fat_time_to_unix(dir_entry.access_date, 0);
    
    // Fill stat structure
    buf->st_mode = mode;
    buf->st_size = dir_entry.size;
    buf->st_blocks = (dir_entry.size + 511) / 512;  // Round up to 512-byte blocks
    buf->st_blksize = 512;
    buf->st_nlink = 1;
    buf->st_uid = 0;
    buf->st_gid = 0;
    //buf->st_atime = atime;
    //buf->st_mtime = mtime;
    //buf->st_ctime = mtime;  // FAT doesn't have creation time in same format
    return FILE_SUCCESS;
    return 0;
}