#pragma once

#include <stdbool.h>

void settings_init(void);

void settings_set_serial_number(const char *str);
char *settings_get_serial_number(void);

void settings_set_input_current_limit_ma(unsigned int current_ma);
unsigned int settings_get_input_current_limit_ma(void);
