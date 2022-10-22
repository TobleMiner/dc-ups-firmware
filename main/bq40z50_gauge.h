#pragma once

#include <esp_err.h>

#include "i2c_bus.h"

typedef struct bq40z50 {
	i2c_bus_t *bus;
	unsigned int address;
} bq40z50_t;

typedef enum {
	BQ40Z50_CELL_1,
	BQ40Z50_CELL_2
} bq40z50_cell_t;

esp_err_t bq40z50_init(bq40z50_t *gauge, i2c_bus_t *bus, int address);

esp_err_t bq40z50_get_battery_voltage_mv(bq40z50_t *gauge, unsigned int *res);
esp_err_t bq40z50_get_cell_voltage_mv(bq40z50_t *gauge, bq40z50_cell_t cell, unsigned int *res);
esp_err_t bq40z50_get_state_of_charge_percent(bq40z50_t *gauge, unsigned int *res);
esp_err_t bq40z50_get_current_ma(bq40z50_t *gauge, bq40z50_cell_t cell, int *res);
esp_err_t bq40z50_shutdown(bq40z50_t *gauge);

