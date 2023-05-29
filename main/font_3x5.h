#pragma once

#include "fb.h"

typedef struct font_vec {
	int x;
	int y;
} font_vec_t;

typedef struct font_fb {
	uint8_t *pixels;
	unsigned int stride;
	font_vec_t size;
} font_fb_t;

typedef struct font_text_params {
	font_vec_t effective_size;
} font_text_params_t;

void font_3x5_render_string(const char *str, fb_t *fb, unsigned int x, unsigned int y);

void font_3x5_calculate_text_params(const char *str, font_text_params_t *params);

void font_3x5_render_string2(const char *str, const font_text_params_t *params, const font_vec_t *source_offset, const font_fb_t *fb);
