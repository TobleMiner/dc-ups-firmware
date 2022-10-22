#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct fb {
	uint8_t data[6 * 64];
} fb_t;

static inline unsigned int fb_width(fb_t *fb) {
	return 64;
}

static inline unsigned int fb_height(fb_t *fb) {
	return 48;
}

static inline void fb_init(fb_t *fb) {
	memset(fb->data, 0, sizeof(fb->data));
}

static inline void fb_set_pixel(fb_t *fb, unsigned int x, unsigned int y, bool val) {
	unsigned int offset = (y / 8) * fb_width(fb) + x;
	if (val) {
		fb->data[offset] |= (1 << (y % 8));
	} else {
		fb->data[offset] &= ~(1 << (y % 8));
	}
}
