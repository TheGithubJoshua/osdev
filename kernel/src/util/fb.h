#pragma once
#include <stdint.h>
#include "../flanterm/flanterm.h"

//static struct flanterm_context *ft_ctx = NULL;

struct fb_info {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
};

void draw(uint32_t x, uint32_t y, uint8_t glyph[16], uint32_t fg_color);
void draw_text(uint32_t x, uint32_t y, const char *text, uint32_t fg_color);

void init_flanterm();
void draw_image(uint32_t *img, int img_w, int img_h, int x, int y);
struct flanterm_context *flanterm_get_ctx();
void uint16_to_hex(uint16_t val, char *out);
void uint64_to_hex(uint16_t val, char *out);
uint32_t* get_fb_addr();
uint64_t get_fb_size();
struct fb_info get_fb_info();
void flanterm_puthex(uint64_t val);
