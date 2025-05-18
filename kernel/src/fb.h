#include <stdint.h>
#include "flanterm/flanterm.h"

extern struct flanterm_context *ft_ctx;

void draw(uint32_t x, uint32_t y, uint8_t glyph[16], uint32_t fg_color);
void draw_text(uint32_t x, uint32_t y, const char *text, uint32_t fg_color);
void write(const char *str, size_t len);