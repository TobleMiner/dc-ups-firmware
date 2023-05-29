#include "font_3x5.h"

#include <stdint.h>
#include <string.h>

#include "util.h"

#define GLYPH_BASE 33

static const uint16_t glyphs[] = {
	0x4124, 0x002D, 0x5F7D, 0x7DDF, 0x52A5, 0x73CB, 0x0024, 0x4494, 0x2922, 
	0x0155, 0x05D0, 0x4C00, 0x01C0, 0x4000, 0x12A4, 0x2B6A, 0x749A, 0x72A3, 
	0x79A7, 0x49ED, 0x39CF, 0x7BCE, 0x12A7, 0x7BEF, 0x39EF, 0x0820, 0x4C20, 
	0x4454, 0x0E38, 0x1511, 0x21A7, 0x736F, 0x5BEF, 0x3AEB, 0x624E, 0x3B6B, 
	0x73CF, 0x13CF, 0x6B4E, 0x5BED, 0x7497, 0x7B24, 0x5AED, 0x7249, 0x5B7D, 
	0x5B6F, 0x7B6F, 0x13EF, 0x76DB, 0x5AEB, 0x388E, 0x2497, 0x7B6D, 0x3B6D, 
	0x5F6D, 0x5AAD, 0x25ED, 0x72A7, 0x6496, 0x4889, 0x6926, 0x016A, 0x7000, 
	0x0022, 0x7BE7, 0x7BC9, 0x73C0, 0x7BE4, 0x73EF, 0x25D6, 0x79FC, 0x5BC9, 
	0x4904, 0x7904, 0x5749, 0x6493, 0x5FC0, 0x5BC0, 0x7BC0, 0x1F78, 0x4F78, 
	0x13C0, 0x7CF8, 0x24BA, 0x7B40, 0x3B40, 0x7F40, 0x5540, 0x79ED, 0x77B8, 
	0x6456, 0x4924, 0x3513, 0x001E, 
};

static uint16_t get_glyph_data(char glyph) {
	uint16_t glyph_data = 0;

	if (glyph >= GLYPH_BASE && glyph - GLYPH_BASE < ARRAY_SIZE(glyphs)) {
		unsigned int glyph_index = glyph - GLYPH_BASE;
		glyph_data = glyphs[glyph_index];
	}

	return glyph_data;
}

static void font_3x5_render_glyph(char glyph, fb_t *fb, unsigned int x, unsigned int y) {
	uint16_t glyph_data = get_glyph_data(glyph);
	unsigned int bit = 0;

	for (unsigned int off_y = 0; off_y < 5; off_y++) {
		for (unsigned int off_x = 0; off_x < 3; off_x++) {
			fb_set_pixel(fb, x + off_x, y + off_y, !!(glyph_data & (1 << bit)));
			bit++;
		}
	}
}

void font_3x5_render_string(const char *str, fb_t *fb, unsigned int x, unsigned int y) {
	while (*str) {
		font_3x5_render_glyph(*str++, fb, x, y);
		x += 4;
	}
}

void font_3x5_calculate_text_params(const char *str, font_text_params_t *params) {
	unsigned int len = strlen(str) * 4;

	if (len > 0) {
		len--;
	}

	params->effective_size.x = len;
	params->effective_size.y = 5;
}

void font_3x5_render_string2(const char *str, const font_text_params_t *params, const font_vec_t *source_offset, const font_fb_t *fb) {
	font_vec_t pos = { 0, 0 };

	while (*str) {
		char c = *str++;
		int dst_x = pos.x - source_offset->x;
		int dst_y = pos.y - source_offset->y;
		unsigned int offset_x = 0;
		uint16_t glyph_data = get_glyph_data(c);

		if (dst_x < 0) {
			offset_x += -dst_x;
			dst_x = 0;
		}
		if (dst_x < fb->size.x && dst_y < fb->size.y && offset_x < 3) {
			unsigned int draw_width = MIN(3, fb->size.x - dst_x);
			unsigned int draw_height = MIN(5, fb->size.y - dst_y);
			unsigned int y = 0;

			if (dst_y < 0) {
				y = -dst_y;
			}
			for (; y < draw_height; y++) {
				unsigned int x = offset_x;

				for (; x < draw_width - offset_x; x++) {
					unsigned int bit = y * 3 + x;

					if (glyph_data & (1 << bit)) {
						fb->pixels[(dst_y + y) * fb->stride + dst_x + x] = 0xff;
					} else {
						fb->pixels[(dst_y + y) * fb->stride + dst_x + x] = 0;
					}
				}
			}
		}

		pos.x += 4;
	}
}
