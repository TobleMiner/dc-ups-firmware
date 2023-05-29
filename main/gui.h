#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "list.h"

typedef struct gui_point {
	int x;
	int y;
} gui_point_t;

typedef struct gui_area {
	gui_point_t position;
	gui_point_t size;
} gui_area_t;

typedef uint8_t gui_pixel_t;
#define GUI_INVERT_PIXEL(px_) (((1U << (sizeof(gui_pixel_t) * 8)) - 1) - (px_))
#define GUI_COLOR_BLACK	0

typedef struct gui_fb {
	gui_pixel_t *pixels;
	unsigned int stride;
} gui_fb_t;

typedef struct gui_element gui_element_t;

typedef struct gui_element_ops {
	int (*render)(gui_element_t *element, const gui_point_t *source_offset, const gui_fb_t *fb, const gui_point_t *destination_size);
	void (*update_shown)(gui_element_t *element);
	void (*check_render)(gui_element_t *element);
} gui_element_ops_t;

typedef struct gui_element {
	// Managed properties
	struct list_head list;
	gui_element_t *parent;
	bool shown;
	bool dirty;

	// User properties
	bool hidden;
	bool inverted;
	gui_area_t area;
	const gui_element_ops_t *ops;
} gui_element_t;

typedef struct gui_container {
	gui_element_t element;

	// Managed properties
	struct list_head children;
} gui_container_t;

typedef struct gui_list {
	gui_container_t container;

	// Managed properties
	gui_element_t *selected_entry;
	int last_y_scroll_pos;
} gui_list_t;

typedef struct gui_image {
	gui_element_t element;
	const uint8_t *image_data_start;
} gui_image_t;

typedef struct gui_rectangle {
	gui_element_t element;

	bool filled;
	gui_pixel_t color;
} gui_rectangle_t;

typedef enum gui_text_alignment {
	GUI_TEXT_ALIGN_START,
	GUI_TEXT_ALIGN_END,
	GUI_TEXT_ALIGN_CENTER,
} gui_text_alignment_t;

typedef struct gui_label {
	gui_element_t element;

	const char *text;
	gui_point_t text_offset;
	gui_text_alignment_t align;
} gui_label_t;

typedef struct gui_marquee {
	gui_container_t container;

	// Managed properties
	int x_scroll_pos;
} gui_marquee_t;

typedef struct gui gui_t;

typedef struct gui_ops {
	void (*request_render)(const gui_t *gui);
} gui_ops_t;

struct gui {
	gui_container_t container;

	SemaphoreHandle_t lock;
	StaticSemaphore_t lock_buffer;
	void *priv;
	const gui_ops_t *ops;
};

// Top level GUI API
gui_element_t *gui_init(gui_t *gui, void *priv, const gui_ops_t *ops);
int gui_render(gui_t *gui, gui_pixel_t *fb, unsigned int stride, const gui_point_t *size);
void gui_lock(gui_t *gui);
void gui_unlock(gui_t *gui);

// Container level GUI API
gui_element_t *gui_container_init(gui_container_t *container);

// Element level GUI API
void gui_element_set_position(gui_element_t *elem, unsigned int x, unsigned int y);
void gui_element_set_size(gui_element_t *elem, unsigned int width, unsigned int height);
void gui_element_set_hidden(gui_element_t *elem, bool hidden);
void gui_element_set_inverted(gui_element_t *elem, bool inverted);
void gui_element_show(gui_element_t *elem);
void gui_element_add_child(gui_element_t *parent, gui_element_t *child);
void gui_element_remove_child(gui_element_t *parent, gui_element_t *child);

// GUI image widget API
gui_element_t *gui_image_init(gui_image_t *image, unsigned int width, unsigned int height, const uint8_t *image_data_start);
void gui_image_set_image(gui_image_t *image, unsigned int width, unsigned int height, const uint8_t *image_data_start);

// GUI list widget API
gui_element_t *gui_list_init(gui_list_t *list);
void gui_list_set_selected_entry(gui_list_t *list, gui_element_t *entry);

// GUI rectangle widget API
gui_element_t *gui_rectangle_init(gui_rectangle_t *rectangle);
void gui_rectangle_set_filled(gui_rectangle_t *rectangle, bool filled);
void gui_rectangle_set_color(gui_rectangle_t *rectangle, gui_pixel_t color);

// GUI label widget API
gui_element_t *gui_label_init(gui_label_t *label, const char *text);
void gui_label_set_text(gui_label_t *label, const char *text);
void gui_label_set_text_alignment(gui_label_t *label, gui_text_alignment_t align);
void gui_label_set_text_offset(gui_label_t *label, int offset_x, int offset_y);

// GUI marquee widget API
gui_element_t *gui_marquee_init(gui_marquee_t *marquee);
