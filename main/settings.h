#pragma once

#include <stdbool.h>

void settings_init(void);

void settings_set_serial_number(const char *str);
char *settings_get_serial_number(void);
