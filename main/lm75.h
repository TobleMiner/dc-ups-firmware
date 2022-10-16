#pragma once

#include <stdint.h>

#include <driver/i2c.h>

#include "i2c_bus.h"

typedef struct lm75 {
	uint8_t address;
	i2c_bus_t *bus;
	uint8_t xfers[I2C_LINK_RECOMMENDED_SIZE(4)];
} lm75_t;

void lm75_init(lm75_t *lm75, i2c_bus_t *bus, unsigned int address);
int32_t lm75_read_temperature_mdegc(lm75_t *lm75);
