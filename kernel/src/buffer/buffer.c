#include "buffer.h"

linebuf_t in;

void setup_linebuffer() {
	lb_init(&in);
}

linebuf_t* fetch_linebuffer() {
	return &in;
}