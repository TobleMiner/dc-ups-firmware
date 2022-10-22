#pragma once

#include <stdint.h>

#include <driver/i2c.h>

#include "i2c_bus.h"

typedef struct ssd1306_oled {
	i2c_bus_t *bus;
	unsigned int address;
	int reset_gpio;
	uint8_t xfers_cmd[I2C_LINK_RECOMMENDED_SIZE(3)];
} ssd1306_oled_t;

esp_err_t ssd1306_oled_init(ssd1306_oled_t *oled, i2c_bus_t *bus, unsigned int address, int reset_gpio);
