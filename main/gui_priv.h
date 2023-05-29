#pragma once

#include "gui.h"

gui_element_t *gui_element_init(gui_element_t *elem, const gui_element_ops_t *ops);

void gui_element_invalidate(gui_element_t *elem);
void gui_element_check_render(gui_element_t *elem);
