#include "gui.h"

#include <stddef.h>

#include <esp_log.h>

#include "font_3x5.h"
#include "gui_priv.h"
#include "util.h"

static const char *TAG = "gui";

gui_element_t *gui_element_init(gui_element_t *elem, const gui_element_ops_t *ops) {
	INIT_LIST_HEAD(elem->list);
	elem->parent = NULL;
	elem->inverted = false;
	elem->ops = ops;
	return elem;
}

void gui_element_check_render(gui_element_t *elem) {
	if (elem->dirty) {
		if (elem->ops->check_render) {
			elem->ops->check_render(elem);
		} else if (elem->parent) {
			gui_element_check_render(elem->parent);
		}
	}
}

static void gui_element_invalidate_ignore_hidden_shown(gui_element_t *elem) {
	elem->dirty = true;
	if (elem->parent) {
		gui_element_invalidate(elem->parent);
	}
}

static void gui_element_invalidate_ignore_hidden(gui_element_t *elem) {
	if (elem->shown) {
		gui_element_invalidate_ignore_hidden_shown(elem);
	}
}

void gui_element_invalidate(gui_element_t *elem) {
	if (!elem->hidden) {
		gui_element_invalidate_ignore_hidden(elem);
	}
}

static void gui_fb_invert_area(const gui_fb_t *fb, const gui_area_t *area) {
	unsigned int x, y;

	for (y = 0; y < area->size.y; y++) {
		for (x = 0; x < area->size.x; x++) {
			unsigned int index = y * fb->stride + x;

			fb->pixels[index] = GUI_INVERT_PIXEL(fb->pixels[index]);
		}
	}
}

static int gui_element_render(gui_element_t *elem, const gui_point_t *source_offset, const gui_fb_t *fb, const gui_point_t *destination_size) {
	int ret = -1;
	if (elem->shown && !elem->hidden && elem->ops->render) {
		ret = elem->ops->render(elem, source_offset, fb, destination_size);
	}

	if (elem->inverted) {
		gui_area_t invert_area = {
			.position = { 0, 0 },
			.size = *destination_size
		};

		invert_area.size.x = MIN(invert_area.size.x, elem->area.size.x);
		invert_area.size.y = MIN(invert_area.size.y, elem->area.size.y);

		gui_fb_invert_area(fb, &invert_area);
	}

	elem->dirty = false;
	return ret;
}

static void gui_element_set_shown(gui_element_t *element, bool shown) {
	element->shown = shown;
	if (element->ops->update_shown) {
		element->ops->update_shown(element);
	}
	gui_element_invalidate_ignore_hidden_shown(element);
}

static int gui_container_render(gui_element_t *element, const gui_point_t *source_offset, const gui_fb_t *fb, const gui_point_t *destination_size) {
	int ret = -1;
	gui_container_t *container = container_of(element, gui_container_t, element);

	ESP_LOGD(TAG, "Rendering container from [%d, %d] to [%d, %d]...", source_offset->x, source_offset->y, destination_size->x, destination_size->y);

	gui_element_t *cursor;
	LIST_FOR_EACH_ENTRY(cursor, &container->children, list) {
		// Render area realtive to container
		gui_area_t render_area = cursor->area;
		gui_point_t local_source_offset = *source_offset;
		gui_fb_t local_fb = {
			.stride = fb->stride
		};
		int retval;

		// Check if there is anything to render
		if (local_source_offset.x >= render_area.position.x + render_area.size.x ||
		    local_source_offset.y >= render_area.position.y + render_area.size.y) {
			// Applied offsets collapse element to zero size, no need to render it
			ESP_LOGD(TAG, "Source offset skips over render area, nothing to do");
			continue;
		}

		if (render_area.position.x >= local_source_offset.x + destination_size->x ||
		    render_area.position.y >= local_source_offset.y + destination_size->y) {
			// Applied offsets collapse element to zero size, no need to render it
			ESP_LOGD(TAG, "Destination area ends before render area starts, nothing to do");
			continue;
		}

		// Clip render area by destination area
		if (render_area.position.x + render_area.size.x - local_source_offset.x > destination_size->x) {
			render_area.size.x = destination_size->x - render_area.position.x + local_source_offset.x;
			ESP_LOGD(TAG, "Limiting horizontal size of render area to %d", render_area.size.x);
		}
		if (render_area.position.y + render_area.size.y - local_source_offset.y > destination_size->y) {
			render_area.size.y = destination_size->y - render_area.position.y + local_source_offset.y;
			ESP_LOGD(TAG, "Limiting vertical size of render area to %d", render_area.size.y);
		}

		// Calculate effective rendering area position relative to fb
		if (render_area.position.x < local_source_offset.x) {
			render_area.size.x -= local_source_offset.x - render_area.position.x;
			local_source_offset.x -= render_area.position.x;
			render_area.position.x = 0;
		} else {
			render_area.position.x -= local_source_offset.x;
			local_source_offset.x = 0;
		}

		if (render_area.position.y < local_source_offset.y) {
			render_area.size.y -= local_source_offset.y - render_area.position.y;
			local_source_offset.y -= render_area.position.y;
			render_area.position.y = 0;
		} else {
			render_area.position.y -= local_source_offset.y;
			local_source_offset.y = 0;
		}

		local_fb.pixels = &fb->pixels[render_area.position.y * fb->stride + render_area.position.x];

		retval = gui_element_render(cursor, &local_source_offset, &local_fb, &render_area.size);
		if (ret == -1) {
			ret = retval;
		} else if (retval != -1) {
			ret = MIN(ret, retval);
		}
	}

	return ret;
}

static void gui_container_update_shown(gui_element_t *element) {
	gui_container_t *container = container_of(element, gui_container_t, element);

	gui_element_t *cursor;
	LIST_FOR_EACH_ENTRY(cursor, &container->children, list) {
		gui_element_set_shown(cursor, container->element.shown);
	}
}

static const gui_element_ops_t gui_container_ops = {
	.render = gui_container_render,
	.update_shown = gui_container_update_shown,
};

static gui_element_t *gui_container_init_(gui_container_t *container, const gui_element_ops_t *ops) {
	gui_element_init(&container->element, ops);
	INIT_LIST_HEAD(container->children);
	return &container->element;
}

gui_element_t *gui_container_init(gui_container_t *container) {
	return gui_container_init_(container, &gui_container_ops);
}

static void gui_element_set_size_(gui_element_t *elem, unsigned int width, unsigned int height) {
	elem->area.size.x = width;
	elem->area.size.y = height;
	gui_element_invalidate(elem);
}

static int gui_list_render(gui_element_t *element, const gui_point_t *source_offset, const gui_fb_t *fb, const gui_point_t *destination_size) {
	int ret = -1;
	gui_list_t *list = container_of(element, gui_list_t, container.element);
	gui_element_t *selected_entry = list->selected_entry;

	ESP_LOGD(TAG, "Rendering list from [%d, %d] to [%d, %d]...", source_offset->x, source_offset->y, destination_size->x, destination_size->y);

	if (selected_entry) {
		gui_area_t *area = &selected_entry->area;

		if (list->last_y_scroll_pos > area->position.y) {
			// Top of entry would be clipped, scroll to show it
			list->last_y_scroll_pos = area->position.y - 1;
		} else if (area->position.y + area->size.y - list->last_y_scroll_pos + 1 >= destination_size->y) {
			// Bottom of entry would be clipped, scroll to show it
			list->last_y_scroll_pos = area->position.y + area->size.y - destination_size->y + 1;
		}
		ESP_LOGD(TAG, "Need to scroll by %d pixels to show selected entry", list->last_y_scroll_pos);
	}

	gui_element_t *cursor;
	LIST_FOR_EACH_ENTRY(cursor, &list->container.children, list) {
		// Render area relative to list
		gui_point_t scrolled_source_offset = *source_offset;
		gui_area_t render_area = cursor->area;
		gui_fb_t local_fb = {
			.stride = fb->stride
		};
		int retval;

		ESP_LOGD(TAG, "Entry size: %dx%d", render_area.size.x, render_area.size.y);

		scrolled_source_offset.y += list->last_y_scroll_pos;

		// Check if there is anything to render
		if (scrolled_source_offset.x >= render_area.position.x + render_area.size.x ||
		    scrolled_source_offset.y >= render_area.position.y + render_area.size.y) {
			// Element outside render area, no need to render it
			ESP_LOGD(TAG, "Source offset skips over render area, nothing to do");
			continue;
		}

		if (render_area.position.x >= scrolled_source_offset.x + destination_size->x ||
		    render_area.position.y >= scrolled_source_offset.y + destination_size->y) {
			// Element outside render area, no need to render it
			ESP_LOGD(TAG, "Destination area ends before render area starts, nothing to do");
			continue;
		}

		// Calculate effective rendering area position relative to fb
		if (render_area.position.x < scrolled_source_offset.x) {
			render_area.size.x -= scrolled_source_offset.x - render_area.position.x;
			scrolled_source_offset.x -= render_area.position.x;
			render_area.position.x = 0;
		} else {
			render_area.position.x -= scrolled_source_offset.x;
			scrolled_source_offset.x = 0;
		}

		if (render_area.position.y < scrolled_source_offset.y) {
			render_area.size.y -= scrolled_source_offset.y - render_area.position.y;
			scrolled_source_offset.y -= render_area.position.y;
			render_area.position.y = 0;
		} else {
			render_area.position.y -= scrolled_source_offset.y;
			scrolled_source_offset.y = 0;
		}

		local_fb.pixels = &fb->pixels[render_area.position.y * fb->stride + render_area.position.x];

		// Clip rendering area size by destination area
		if (render_area.position.x + render_area.size.x - scrolled_source_offset.x > destination_size->x) {
			render_area.size.x = destination_size->x - render_area.position.x + scrolled_source_offset.x;
			ESP_LOGD(TAG, "Limiting horizontal size of render area to %d", render_area.size.x);
		}
		if (render_area.position.y + render_area.size.y - scrolled_source_offset.y > destination_size->y) {
			render_area.size.y = destination_size->y - render_area.position.y + scrolled_source_offset.y;
			ESP_LOGD(TAG, "Limiting vertical size of render area to %d", render_area.size.y);
		}

		retval = gui_element_render(cursor, &scrolled_source_offset, &local_fb, &render_area.size);
		if (ret == -1) {
			ret = retval;
		} else if (retval != -1) {
			ret = MIN(ret, retval);
		}

		// Highlight selected entry by inverting its pixels
		if (cursor == list->selected_entry) {
			gui_area_t invert_area = {
				.position = { 0, 0 },
				.size = render_area.size
			};

			ESP_LOGD(TAG, "Inverting %dx%d area", render_area.size.x, render_area.size.y);

			gui_fb_invert_area(&local_fb, &invert_area);
		}
	}

	return ret;
}

static const gui_element_ops_t gui_list_ops = {
	.render = gui_list_render,
	.update_shown = gui_container_update_shown,
};

gui_element_t *gui_list_init(gui_list_t *list) {
	gui_container_init_(&list->container, &gui_list_ops);
	list->selected_entry = NULL;
	list->last_y_scroll_pos = 0;
	return &list->container.element;
}

static int gui_image_render(gui_element_t *element, const gui_point_t *source_offset, const gui_fb_t *fb, const gui_point_t *destination_size) {
	gui_image_t *image = container_of(element, gui_image_t, element);
	int copy_width = MIN(element->area.size.x - source_offset->x, destination_size->x);
	int copy_height = MIN(element->area.size.y - source_offset->y, destination_size->y);
	unsigned int y;

	ESP_LOGD(TAG, "Rendering image from [%d, %d] to [%d, %d]...", source_offset->x, source_offset->y, destination_size->x, destination_size->y);

	for (y = 0; y < copy_height; y++) {
		gui_pixel_t *dst = &fb->pixels[y * fb->stride];
		const uint8_t *src = &image->image_data_start[image->element.area.size.x * (y + source_offset->y) + source_offset->x];

		memcpy(dst, src, copy_width * sizeof(gui_pixel_t));
	}

	return -1;
}

static const gui_element_ops_t gui_image_ops = {
	.render = gui_image_render,
};

gui_element_t *gui_image_init(gui_image_t *image, unsigned int width, unsigned int height, const uint8_t *image_data_start) {
	gui_element_init(&image->element, &gui_image_ops);
	image->element.ops = &gui_image_ops;
	gui_element_set_size_(&image->element, width, height);
	image->image_data_start = image_data_start;
	return &image->element;
}

static void gui_fb_memset(const gui_fb_t *fb, gui_pixel_t color, const gui_point_t *size) {
	unsigned int x, y;

	for (y = 0; y < size->y; y++) {
		for (x = 0; x < size->x; x++) {
			fb->pixels[y * fb->stride + x] = color;
		}
	}
}

int gui_render(gui_t *gui, gui_pixel_t *fb, unsigned int stride, const gui_point_t *size) {
	int ret;
	const gui_point_t offset = { 0, 0 };
	gui_fb_t gui_fb = {
		.pixels = fb,
		.stride = stride
	};

	gui_fb_memset(&gui_fb, GUI_COLOR_BLACK, size);
	ret = gui_container_render(&gui->container.element, &offset, &gui_fb, size);
	gui->container.element.dirty = false;
	return ret;
}

static void gui_check_render(gui_element_t *element) {
	gui_t *gui = container_of(element, gui_t, container.element);

	if (gui->ops->request_render) {
		gui->ops->request_render(gui);
	}
}

static const gui_element_ops_t gui_ops = {
	.check_render = gui_check_render,
};

gui_element_t *gui_init(gui_t *gui, void *priv, const gui_ops_t *ops) {
	gui_container_init(&gui->container);
	gui->container.element.ops = &gui_ops;
	gui->container.element.hidden = false;
	gui->container.element.shown = true;
	gui->priv = priv;
	gui->ops = ops;
	gui->lock = xSemaphoreCreateMutexStatic(&gui->lock_buffer);
	return &gui->container.element;
}

void gui_lock(gui_t *gui) {
	xSemaphoreTake(gui->lock, portMAX_DELAY);
}

void gui_unlock(gui_t *gui) {
	xSemaphoreGive(gui->lock);
}

static int gui_rectangle_render(gui_element_t *element, const gui_point_t *source_offset, const gui_fb_t *fb, const gui_point_t *destination_size) {
	gui_rectangle_t *rect = container_of(element, gui_rectangle_t, element);
	int width = MIN(element->area.size.x - source_offset->x, destination_size->x);
	int height = MIN(element->area.size.y - source_offset->y, destination_size->y);

	ESP_LOGD(TAG, "Rendering rectangle from [%d, %d] to [%d, %d]...", source_offset->x, source_offset->y, destination_size->x, destination_size->y);

	if (rect->filled) {
		gui_point_t fill_size = {
			width,
			height
		};

		gui_fb_memset(fb, rect->color, &fill_size);
	} else {
		unsigned int max_y = element->area.size.y - source_offset->y;
		unsigned int max_x = element->area.size.x - source_offset->x;

		if (source_offset->y == 0) {
			unsigned int x;

			for (x = 0; x < width; x++) {
				fb->pixels[x] = rect->color;
			}
		}

		if (destination_size->y >= max_y) {
			unsigned int x;

			for (x = 0; x < width; x++) {
				fb->pixels[(max_y - 1) * fb->stride + x] = rect->color;
			}
		}

		if (source_offset->x == 0) {
			unsigned int y;

			for (y = 0; y < height; y++) {
				fb->pixels[y * fb->stride] = rect->color;
			}
		}

		if (destination_size->x >= max_x) {
			unsigned int y;

			for (y = 0; y < height; y++) {
				fb->pixels[y * fb->stride + max_x - 1] = rect->color;
			}
		}
	}

	return -1;
}

static const gui_element_ops_t gui_rectangle_ops = {
	.render = gui_rectangle_render,
};

gui_element_t *gui_rectangle_init(gui_rectangle_t *rectangle) {
	gui_element_init(&rectangle->element, &gui_rectangle_ops);
	return &rectangle->element;
}

static int gui_label_render(gui_element_t *element, const gui_point_t *source_offset, const gui_fb_t *fb, const gui_point_t *destination_size) {
	gui_label_t *label = container_of(element, gui_label_t, element);
	int width = MIN(element->area.size.x - source_offset->x, destination_size->x);
	int height = MIN(element->area.size.y - source_offset->y, destination_size->y);
	font_fb_t font_fb;
	font_vec_t font_source_offset = {
		.x = source_offset->x,
		.y = source_offset->y
	};
	int err;
	int offset_x = label->text_offset.x;
	int offset_y = label->text_offset.y;
	font_text_params_t text_params;

	if (!label->text) {
		return -1;
	}

	font_3x5_calculate_text_params(label->text, &text_params);

	ESP_LOGD(TAG, "Size required to render string: %dx%d px", text_params.effective_size.x, text_params.effective_size.y);

	if (label->align == GUI_TEXT_ALIGN_END) {
		if (width > text_params.effective_size.x) {
			offset_x += width - text_params.effective_size.x;
		}
	} else if (label->align == GUI_TEXT_ALIGN_CENTER) {
		if (width > text_params.effective_size.x) {
			offset_x += (width - text_params.effective_size.x) / 2;
		}
	}

	if (source_offset->x > offset_x) {
		font_source_offset.x -= offset_x;
		offset_x = 0;
	} else {
		offset_x -= source_offset->x;
		font_source_offset.x = 0;
	}
	if (source_offset->y > offset_y) {
		font_source_offset.y -= offset_y;
		offset_y = 0;
	} else {
		offset_y -= source_offset->y;
		font_source_offset.y = 0;
	}

	font_fb.pixels = &fb->pixels[offset_y * fb->stride + offset_x];
	font_fb.stride = fb->stride;
	font_fb.size.x = width - offset_x;
	font_fb.size.y = height - offset_y;

	font_3x5_render_string2(label->text, &text_params, &font_source_offset, &font_fb);

	return -1;
}

static const gui_element_ops_t gui_label_ops = {
	.render = gui_label_render,
};

gui_element_t *gui_label_init(gui_label_t *label, const char *text) {
	gui_element_init(&label->element, &gui_label_ops);
	label->text = text;
	label->text_offset.x = 0;
	label->text_offset.y = 0;
	label->align = GUI_TEXT_ALIGN_START;
	return &label->element;
}

static int gui_marquee_render(gui_element_t *element, const gui_point_t *source_offset, const gui_fb_t *fb, const gui_point_t *destination_size) {
	int ret = 10;
	gui_marquee_t *marquee = container_of(element, gui_marquee_t, container.element);
	unsigned int max_width = 0;
	gui_element_t *cursor;

	ESP_LOGD(TAG, "Rendering marquee from [%d, %d] to [%d, %d]...", source_offset->x, source_offset->y, destination_size->x, destination_size->y);

	LIST_FOR_EACH_ENTRY(cursor, &marquee->container.children, list) {
		max_width = MAX(max_width, cursor->area.size.x);
	}

	if (max_width > marquee->container.element.area.size.x) {
		int delta = max_width - marquee->container.element.area.size.x;

		marquee->x_scroll_pos++;
		marquee->x_scroll_pos %= delta;
	} else {
		marquee->x_scroll_pos = 0;
	}

	LIST_FOR_EACH_ENTRY(cursor, &marquee->container.children, list) {
		// Render area relative to marquee start
		gui_point_t scrolled_source_offset = *source_offset;
		gui_area_t render_area = cursor->area;
		gui_fb_t local_fb = {
			.stride = fb->stride
		};
		int retval;

		ESP_LOGD(TAG, "Entry size: %dx%d", render_area.size.x, render_area.size.y);

		// Check if there is anything to render
		if (scrolled_source_offset.x >= render_area.position.x + render_area.size.x ||
		    scrolled_source_offset.y >= render_area.position.y + render_area.size.y) {
			// Element outside render area, no need to render it
			ESP_LOGD(TAG, "Source offset skips over render area, nothing to do");
			continue;
		}

		if (render_area.position.x >= scrolled_source_offset.x + destination_size->x ||
		    render_area.position.y >= scrolled_source_offset.y + destination_size->y) {
			// Element outside render area, no need to render it
			ESP_LOGD(TAG, "Destination area ends before render area starts, nothing to do");
			continue;
		}

		// Calculate effective rendering area position relative to fb
		if (render_area.position.x < scrolled_source_offset.x) {
			render_area.size.x -= scrolled_source_offset.x - render_area.position.x;
			scrolled_source_offset.x -= render_area.position.x;
			render_area.position.x = 0;
		} else {
			render_area.position.x -= scrolled_source_offset.x;
			scrolled_source_offset.x = 0;
		}

		if (render_area.position.y < scrolled_source_offset.y) {
			render_area.size.y -= scrolled_source_offset.y - render_area.position.y;
			scrolled_source_offset.y -= render_area.position.y;
			render_area.position.y = 0;
		} else {
			render_area.position.y -= scrolled_source_offset.y;
			scrolled_source_offset.y = 0;
		}

		local_fb.pixels = &fb->pixels[render_area.position.y * fb->stride + render_area.position.x];

		// Clip rendering area size by destination area
		if (render_area.position.x + render_area.size.x - scrolled_source_offset.x > destination_size->x) {
			render_area.size.x = destination_size->x - render_area.position.x + scrolled_source_offset.x;
			ESP_LOGD(TAG, "Limiting horizontal size of render area to %d", render_area.size.x);
		}
		if (render_area.position.y + render_area.size.y - scrolled_source_offset.y > destination_size->y) {
			render_area.size.y = destination_size->y - render_area.position.y + scrolled_source_offset.y;
			ESP_LOGD(TAG, "Limiting vertical size of render area to %d", render_area.size.y);
		}

		// Skip part of content during rendering
		scrolled_source_offset.x += marquee->x_scroll_pos;

		retval = gui_element_render(cursor, &scrolled_source_offset, &local_fb, &render_area.size);
		if (retval != -1) {
			ret = MIN(ret, retval);
		}
	}

	return ret;
}

static const gui_element_ops_t gui_marquee_ops = {
	.render = gui_marquee_render,
	.update_shown = gui_container_update_shown,
};

gui_element_t *gui_marquee_init(gui_marquee_t *marquee) {
	gui_container_init_(&marquee->container, &gui_marquee_ops);
	marquee->x_scroll_pos = 0;
	return &marquee->container.element;
}

// User API functions that might require rerendering
void gui_element_set_position(gui_element_t *elem, unsigned int x, unsigned int y) {
	elem->area.position.x = x;
	elem->area.position.y = y;
	gui_element_invalidate(elem);
	gui_element_check_render(elem);
}

void gui_element_set_size(gui_element_t *elem, unsigned int width, unsigned int height) {
	gui_element_set_size_(elem, width, height);
	gui_element_check_render(elem);
}

void gui_element_set_hidden(gui_element_t *elem, bool hidden) {
	elem->hidden = hidden;
	gui_element_invalidate_ignore_hidden(elem);
	gui_element_check_render(elem);
}

void gui_element_set_inverted(gui_element_t *elem, bool inverted) {
	elem->inverted = inverted;
	gui_element_invalidate(elem);
	gui_element_check_render(elem);
}

void gui_element_show(gui_element_t *elem) {
	gui_element_set_shown(elem, true);
	gui_element_check_render(elem);
}

void gui_element_add_child(gui_element_t *parent, gui_element_t *child) {
	gui_container_t *container = container_of(parent, gui_container_t, element);

	child->parent = parent;
	LIST_APPEND_TAIL(&child->list, &container->children);
	gui_element_check_render(parent);
}

void gui_element_remove_child(gui_element_t *parent, gui_element_t *child) {
	gui_element_set_shown(child, false);
	LIST_DELETE(&child->list);
	child->parent = NULL;
	gui_element_check_render(parent);
}

void gui_rectangle_set_filled(gui_rectangle_t *rectangle, bool filled) {
	rectangle->filled = filled;
	gui_element_invalidate(&rectangle->element);
	gui_element_check_render(&rectangle->element);
}

void gui_rectangle_set_color(gui_rectangle_t *rectangle, gui_pixel_t color) {
	rectangle->color = color;
	gui_element_invalidate(&rectangle->element);
	gui_element_check_render(&rectangle->element);
}

void gui_list_set_selected_entry(gui_list_t *list, gui_element_t *entry) {
	list->selected_entry = entry;
	gui_element_invalidate(&list->container.element);
	gui_element_check_render(&list->container.element);
}

void gui_image_set_image(gui_image_t *image, unsigned int width, unsigned int height, const uint8_t *image_data_start) {
	image->image_data_start = image_data_start;
	gui_element_set_size_(&image->element, width, height);
	gui_element_check_render(&image->element);
}

void gui_label_set_text(gui_label_t *label, const char *text) {
	label->text = text;
	gui_element_invalidate(&label->element);
	gui_element_check_render(&label->element);
}

void gui_label_set_text_alignment(gui_label_t *label, gui_text_alignment_t align) {
	label->align = align;
	gui_element_invalidate(&label->element);
	gui_element_check_render(&label->element);
}

void gui_label_set_text_offset(gui_label_t *label, int offset_x, int offset_y) {
	label->text_offset.x = offset_x;
	label->text_offset.y = offset_y;
	gui_element_invalidate(&label->element);
	gui_element_check_render(&label->element);
}
