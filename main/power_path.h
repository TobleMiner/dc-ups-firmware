#pragma once

#include <stdbool.h>

bool power_path_is_running_on_battery(void);
unsigned int power_path_get_dc_output_voltage_mv(void);
bool power_path_is_dc_output_enabled(unsigned int output_idx);
