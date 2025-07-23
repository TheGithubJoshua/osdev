#ifndef _ERRNO_H
#define _ERRNO_H

/* Success */
#define EOK             0   /* No error */

/* General errors */
#define EPERM           1   /* Operation not permitted */
#define ENOENT          2   /* No such file or directory */
#define ESRCH           3   /* No such process */
#define EINTR           4   /* Interrupted system call */
#define EIO             5   /* I/O error */
#define ENXIO           6   /* No such device or address */
#define E2BIG           7   /* Argument list too long */
#define ENOEXEC         8   /* Exec format error */
#define EBADF           9   /* Bad file descriptor */
#define ECHILD          10  /* No child processes */
#define EAGAIN          11  /* Try again / Resource temporarily unavailable */
#define ENOMEM          12  /* Out of memory */
#define EACCES          13  /* Permission denied */
#define EFAULT          14  /* Bad address */
#define ENOTBLK         15  /* Block device required */
#define EBUSY           16  /* Device or resource busy */
#define EEXIST          17  /* File exists */
#define EXDEV           18  /* Cross-device link */
#define ENODEV          19  /* No such device */
#define ENOTDIR         20  /* Not a directory */
#define EISDIR          21  /* Is a directory */
#define EINVAL          22  /* Invalid argument */
#define ENFILE          23  /* File table overflow */
#define EMFILE          24  /* Too many open files */
#define ENOTTY          25  /* Not a typewriter/terminal */
#define ETXTBSY         26  /* Text file busy */
#define EFBIG           27  /* File too large */
#define ENOSPC          28  /* No space left on device */
#define ESPIPE          29  /* Illegal seek */
#define EROFS           30  /* Read-only file system */
#define EMLINK          31  /* Too many links */
#define EPIPE           32  /* Broken pipe */
#define EDOM            33  /* Math argument out of domain */
#define ERANGE          34  /* Math result not representable */

/* Resource/locking errors */
#define EDEADLK         35  /* Resource deadlock would occur */
#define ENAMETOOLONG    36  /* File name too long */
#define ENOLCK          37  /* No record locks available */
#define ENOSYS          38  /* Function not implemented */
#define ENOTEMPTY       39  /* Directory not empty */
#define ELOOP           40  /* Too many symbolic links encountered */

/* Network/IPC errors (if implementing networking) */
#define EWOULDBLOCK     EAGAIN  /* Operation would block */
#define ENOMSG          42  /* No message of desired type */
#define EIDRM           43  /* Identifier removed */
#define ECHRNG          44  /* Channel number out of range */
#define EL2NSYNC        45  /* Level 2 not synchronized */
#define EL3HLT          46  /* Level 3 halted */
#define EL3RST          47  /* Level 3 reset */
#define ELNRNG          48  /* Link number out of range */
#define EUNATCH         49  /* Protocol driver not attached */
#define ENOCSI          50  /* No CSI structure available */
#define EL2HLT          51  /* Level 2 halted */
#define EBADE           52  /* Invalid exchange */
#define EBADR           53  /* Invalid request descriptor */
#define EXFULL          54  /* Exchange full */
#define ENOANO          55  /* No anode */
#define EBADRQC         56  /* Invalid request code */
#define EBADSLT         57  /* Invalid slot */

/* File system specific errors */
#define EBFONT          59  /* Bad font file format */
#define ENOSTR          60  /* Device not a stream */
#define ENODATA         61  /* No data available */
#define ETIME           62  /* Timer expired */
#define ENOSR           63  /* Out of streams resources */
#define ENONET          64  /* Machine is not on the network */
#define ENOPKG          65  /* Package not installed */
#define EREMOTE         66  /* Object is remote */
#define ENOLINK         67  /* Link has been severed */
#define EADV            68  /* Advertise error */
#define ESRMNT          69  /* Srmount error */
#define ECOMM           70  /* Communication error on send */
#define EPROTO          71  /* Protocol error */
#define EMULTIHOP       72  /* Multihop attempted */
#define EDOTDOT         73  /* RFS specific error */
#define EBADMSG         74  /* Not a data message */
#define EOVERFLOW       75  /* Value too large for defined data type */
#define ENOTUNIQ        76  /* Name not unique on network */
#define EBADFD          77  /* File descriptor in bad state */
#define EREMCHG         78  /* Remote address changed */

/* Custom OS-specific errors (starting from 100 to avoid conflicts) */
#define E_KERNEL_PANIC      100 /* Kernel panic condition */
#define E_PAGE_FAULT        101 /* Page fault error */
#define E_SEGMENT_FAULT     102 /* Segmentation fault */
#define E_STACK_OVERFLOW    103 /* Stack overflow */
#define E_HEAP_CORRUPTION   104 /* Heap corruption detected */
#define E_INVALID_SYSCALL   105 /* Invalid system call number */
#define E_PRIVILEGE_VIOLATION 106 /* Privilege level violation */

/* File system specific errors */
#define E_FILE_NOT_FOUND    107 /* File not found (your original) */
#define E_FILE_READ_ERROR   108 /* File read error (your original) */
#define E_FILE_WRITE_ERROR  109 /* File write error */
#define E_FILE_SEEK_ERROR   110 /* File seek error */
#define E_FILE_CORRUPT      111 /* File or filesystem corruption */
#define E_DIRECTORY_FULL    112 /* Directory is full */
#define E_FAT_ERROR         113 /* FAT filesystem error */
#define E_INODE_ERROR       114 /* Inode error */
#define E_PARTITION_ERROR   115 /* Partition table error */

/* Memory management errors */
#define E_OUT_OF_PAGES      116 /* Out of physical pages */
#define E_OUT_OF_VMEM       117 /* Out of virtual memory */
#define E_INVALID_ADDRESS   118 /* Invalid memory address */
#define E_MEMORY_ALIGN      119 /* Memory alignment error */
#define E_DMA_ERROR         120 /* DMA operation failed */

/* Process/task errors */
#define E_INVALID_PID       121 /* Invalid process ID */
#define E_PROCESS_LIMIT     122 /* Process limit exceeded */
#define E_THREAD_LIMIT      123 /* Thread limit exceeded */
#define E_SCHEDULER_ERROR   124 /* Scheduler error */
#define E_CONTEXT_SWITCH    125 /* Context switch failure */

/* Device/driver errors */
#define E_DEVICE_NOT_READY  126 /* Device not ready */
#define E_DEVICE_TIMEOUT    127 /* Device operation timeout */
#define E_DRIVER_ERROR      128 /* Generic driver error */
#define E_IRQ_ERROR         129 /* Interrupt handling error */
#define E_DMA_TIMEOUT       130 /* DMA timeout */

/* Synchronization errors */
#define E_MUTEX_ERROR       131 /* Mutex operation failed */
#define E_SEMAPHORE_ERROR   132 /* Semaphore operation failed */
#define E_SPINLOCK_ERROR    133 /* Spinlock error */
#define E_CONDITION_ERROR   134 /* Condition variable error */

/* Boot/initialization errors */
#define E_BOOT_ERROR        135 /* Boot process error */
#define E_INIT_ERROR        136 /* Initialization error */
#define E_CONFIG_ERROR      137 /* Configuration error */
#define E_HARDWARE_ERROR    138 /* Hardware malfunction */

/* Maximum error number for bounds checking */
#define EMAX                138

/* Helper macros for error handling */
#define IS_ERROR(x)         ((x) < 0)
#define IS_SUCCESS(x)       ((x) >= 0)
#define ERROR_CODE(x)       (-(x))  /* Convert positive error to negative */
#define ABS_ERROR(x)        ((x) < 0 ? -(x) : (x))  /* Get absolute error code */

#endif /* _ERRNO_H */