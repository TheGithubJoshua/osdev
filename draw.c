/* draw_term.c
 * Barebones "drawing" program
 * Moves a white pixel with WASD using the terminal input buffer.
 * Clears screen to black on start.
 * Quit with Q.
 */
typedef unsigned long  u64;
typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;

#define FB_ADDR   ((u8*)0x0000000000FB0000ULL)

struct fb_info {
    u64 width;
    u64 height;
    u64 pitch;
    u16 bpp;
};

/* syscall 11: read terminal buffer (returns packed chars in rax) */
static inline u64 sys_terminal_read(void) {
    u64 ret;
    asm volatile(
        "int $0x69\n\t"
        : "=a"(ret)
        : "a"((u64)11)
        : "memory"
    );
    return ret;
}

/* syscall 17: get framebuffer info */
static inline int sys_get_fb_info(struct fb_info *info) {
    u64 ret;
    asm volatile(
        "int $0x69\n\t"
        : "=a"(ret)
        : "a"((u64)17), "S"(info)
        : "memory"
    );
    return (int)ret;
}

/* draw one pixel */
static inline void put_pixel(u32 x, u32 y, u32 color, u64 pitch, u64 width, u64 height) {
    if (x >= width || y >= height) return;
    u8 *fb = FB_ADDR;
    *(u32*)(fb + (u64)y * pitch + (u64)x * 4) = color;
}

/* fill whole screen with color */
static void clear_screen(u32 color, u64 pitch, u64 width, u64 height) {
    u8 *fb = FB_ADDR;
    for (u64 y = 0; y < height; y++) {
        u32 *row = (u32*)(fb + y * pitch);
        for (u64 x = 0; x < width; x++) {
            row[x] = color;
        }
    }
}

/* delay loop */
static void delay(u32 loops) {
    for (volatile u32 i = 0; i < loops; ++i)
        asm volatile("nop");
}

/* process packed terminal input (8 chars per syscall) */
static int process_input(u64 packed, int *x, int *y, int max_x, int max_y) {
    for (int i = 0; i < 8; ++i) {
        u8 ch = (u8)(packed & 0xFFu);
        packed >>= 8;
        
        if (ch == 0) continue;
        
        if (ch == 'w' || ch == 'W') {
            if (*y > 0) (*y)--;
        } else if (ch == 's' || ch == 'S') {
            if (*y < max_y - 1) (*y)++;
        } else if (ch == 'a' || ch == 'A') {
            if (*x > 0) (*x)--;
        } else if (ch == 'd' || ch == 'D') {
            if (*x < max_x - 1) (*x)++;
        } else if (ch == 'q' || ch == 'Q') {
            return 1; /* quit */
        }
    }
    return 0;
}

/* entry point */
void draw_main(void) {
    struct fb_info info;
    
    /* Get framebuffer info from kernel */
    if (sys_get_fb_info(&info) != 0) {
        return; /* Failed to get fb info */
    }
    
    int x = info.width / 2;
    int y = info.height / 2;
    
    clear_screen(0x00000000u, info.pitch, info.width, info.height); /* black */
    
    for (;;) {
        u64 packed = sys_terminal_read();
        if (packed) {
            int quit = process_input(packed, &x, &y, (int)info.width, (int)info.height);
            if (quit) break;
        }
        
        put_pixel(x, y, 0x00FFFFFFu, info.pitch, info.width, info.height); /* white */
        delay(50000u);
    }
    
    /* clear screen on exit */
    clear_screen(0x00000000u, info.pitch, info.width, info.height);
}
