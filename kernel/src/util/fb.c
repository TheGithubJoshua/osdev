#include "fb.h"
#include "../flanterm/flanterm.h"
#include "../flanterm/backends/fb.h"
#include <limine.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// Declare it static to limit scope to fb.c
static struct flanterm_context *ft_ctx = NULL;

void init_flanterm() {
struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    ft_ctx = flanterm_fb_init(
        NULL, NULL,
        fb->address, fb->width, fb->height, fb->pitch,
        fb->red_mask_size, fb->red_mask_shift,
        fb->green_mask_size, fb->green_mask_shift,
        fb->blue_mask_size, fb->blue_mask_shift,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, 0, 0, 1,
        0, 0, 0
    );
}

void draw_image(uint32_t *img, int img_w, int img_h, int x, int y) {
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    uint32_t *fb_ptr = (uint32_t *)fb->address;
    
    // Boundary checking
    if (x < 0 || y < 0 || (x + img_w) > fb->width || (y + img_h) > fb->height) {
        return;  // Image would be drawn outside framebuffer
    }

    for (int dy = 0; dy < img_h; dy++) {
        for (int dx = 0; dx < img_w; dx++) {
            int fb_pos = (y + dy) * fb->width + (x + dx);
            int img_pos = dy * img_w + dx;
            fb_ptr[fb_pos] = img[img_pos];
        }
    }
}

struct flanterm_context *flanterm_get_ctx() {
    return ft_ctx;
}

void uint16_to_hex(uint16_t val, char *out) {
    const char *hex = "0123456789ABCDEF";
    for (int i = 0; i < 4; ++i) {
        out[3 - i] = hex[val & 0xF];
        val >>= 4;
    }
    out[4] = '\0'; // null-terminate
}

void uint64_to_hex(uint16_t val, char *out) {
    const char *hex = "0123456789ABCDEF";
    for (int i = 0; i < 15; ++i) {
        out[3 - i] = hex[val & 0xF];
        val >>= 15;
    }
    out[15] = '\0'; // null-terminate
}

uint32_t* get_fb_addr() {
  struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
  uint32_t *fb_ptr = (uint32_t *)fb->address;
  return fb_ptr;
}

uint64_t get_fb_size() {
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    return fb->width*fb->height*fb->bpp;
}


