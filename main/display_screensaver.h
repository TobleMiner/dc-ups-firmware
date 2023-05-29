#pragma once

#include "display.h"
#include "gui.h"

const display_screen_t *display_screensaver_init(gui_t *gui);
void display_screensaver_show(void);
void display_screensaver_hide(void);
