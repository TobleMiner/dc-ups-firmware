#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "i2c_bus.h"
#include "smbus.h"

typedef enum power_path_group {
	POWER_PATH_GROUP_IN,
	POWER_PATH_GROUP_DC,
	POWER_PATH_GROUP_USB,
	POWER_PATH_GROUP_MAX_ = POWER_PATH_GROUP_USB
} power_path_group_t;

typedef struct power_path_group_data {
	unsigned int voltage_mv;
	int current_ma;
	long power_mw;
	int32_t temperature_mdegc;
} power_path_group_data_t;

void power_path_early_init(smbus_t *smbus, i2c_bus_t *i2c_bus);
void power_path_init(smbus_t *smbus, i2c_bus_t *i2c_bus);
void power_path_set_input_current_limit(unsigned int current_ma);
unsigned int power_path_get_input_current_limit_ma(void);
bool power_path_is_running_on_battery(void);
unsigned int power_path_get_dc_output_voltage_mv(void);
bool power_path_is_dc_output_enabled(unsigned int output_idx);
unsigned long power_path_get_output_power_consumption_mw(void);
void power_path_get_group_data(power_path_group_t group, power_path_group_data_t *data);
