#include <stdint.h>
#include "../flanterm/flanterm.h"

//static struct flanterm_context *ft_ctx = NULL;

void draw(uint32_t x, uint32_t y, uint8_t glyph[16], uint32_t fg_color);
void draw_text(uint32_t x, uint32_t y, const char *text, uint32_t fg_color);
void write(const char *str, size_t len);

void init_flanterm();
void draw_image(uint32_t *img, int img_w, int img_h, int x, int y);
struct flanterm_context *flanterm_get_ctx();
void uint16_to_hex(uint16_t val, char *out);
void uint64_to_hex(uint16_t val, char *out);

