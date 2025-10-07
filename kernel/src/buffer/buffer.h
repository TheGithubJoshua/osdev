#include <stdint.h>
#include <stddef.h>

#ifndef LB_SIZE
#define LB_SIZE 128
#endif

typedef struct {
    char buf[LB_SIZE];
    size_t len; /* number of valid bytes in buf (0..LB_SIZE-1) */
} linebuf_t;

static inline void lb_init(linebuf_t *b) {
    b->len = 0;
}

/* Append a character at the end (like typing).
   Returns 0 on success, -1 if full. */
static inline int lb_append(linebuf_t *b, char c) {
    if (b->len >= (LB_SIZE - 1)) return -1; /* reserve optional NUL if you want */
    b->buf[b->len++] = c;
    return 0;
}

/* Remove last character (backspace). Returns 0 on success, -1 if empty. */
static inline int lb_pop_back(linebuf_t *b) {
    if (b->len == 0) return -1;
    b->len--;
    return 0;
}

/* Read (consume) first character. This shifts remaining bytes forward.
   Returns >=0 (0..255) on success, or -1 if empty. */
static inline int lb_read(linebuf_t *b) {
    if (b->len == 0) return -1;
    unsigned char c = (unsigned char)b->buf[0];

    /* shift remaining bytes left by one */
    for (size_t i = 1; i < b->len; ++i) {
        b->buf[i - 1] = b->buf[i];
    }
    b->len--;
    return (int)c;
}

/* Peek first char without consuming, or -1 if empty */
static inline int lb_peek(const linebuf_t *b) {
    if (b->len == 0) return -1;
    return (int)(unsigned char)b->buf[0];
}

/* Clear buffer */
static inline void lb_clear(linebuf_t *b) { b->len = 0; }

void setup_linebuffer();
linebuf_t* fetch_linebuffer();