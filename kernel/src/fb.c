#include "fb.h"
#include "flanterm/flanterm.h"
#include "flanterm/backends/fb.h"
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

struct flanterm_context *flanterm_get_ctx() {
    return ft_ctx;
}
