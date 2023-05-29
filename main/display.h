#pragma once

#include "i2c_bus.h"

typedef struct display_screen {
	const char *name;
	void (*show)(void);
	void (*hide)(void);
} display_screen_t;

void display_init(i2c_bus_t *display_bus);
void display_render_loop(void);
