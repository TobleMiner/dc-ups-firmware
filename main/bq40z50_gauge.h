#pragma once

#include <esp_err.h>

#include "i2c_bus.h"

typedef struct bq40z50 {
	i2c_bus_t *bus;
	unsigned int address;
} bq40z50_t;

esp_err_t bq40z50_init(bq40z50_t *gauge, i2c_bus_t *bus, int address);
