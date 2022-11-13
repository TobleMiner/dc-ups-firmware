#include "power_path.h"

#include <stdbool.h>
#include <stdint.h>

#include <driver/gpio.h>

#define GPIO_DCOK	34

#define GPIO_VSEL0	35
#define GPIO_VSEL1	36

static const unsigned int dc_output_voltage_table[] = {
	9000,
	12000,
	12500,
	15000
};

static bool dc_output_enable_table[] = {
	true,
	true,
	true
};

bool power_path_is_running_on_battery() {
	gpio_set_direction(GPIO_DCOK, GPIO_MODE_INPUT);
	return !gpio_get_level(GPIO_DCOK);
}

unsigned int power_path_get_dc_output_voltage_mv() {
	gpio_set_direction(GPIO_VSEL0, GPIO_MODE_INPUT);
	gpio_set_direction(GPIO_VSEL1, GPIO_MODE_INPUT);

	unsigned int lookup_idx =
		(gpio_get_level(GPIO_VSEL0) ? 1 : 0) |
		((gpio_get_level(GPIO_VSEL1) ? 1 : 0) << 1);
	return dc_output_voltage_table[lookup_idx];
}

bool power_path_is_dc_output_enabled(unsigned int output_idx) {
	return dc_output_enable_table[output_idx];
}