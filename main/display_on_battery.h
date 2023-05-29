#pragma once

#include "display.h"
#include "gui.h"

const display_screen_t *display_on_battery_init(gui_t *gui);
void display_on_battery_show(void);
void display_on_battery_hide(void);
