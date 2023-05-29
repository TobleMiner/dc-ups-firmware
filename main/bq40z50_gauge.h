#pragma once

#include <esp_err.h>

#include "battery_gauge.h"
#include "smbus.h"
#include "sensor.h"

typedef struct bq40z50 {
	smbus_t *bus;
	unsigned int address;
	sensor_t sensor;
	battery_gauge_t gauge;
} bq40z50_t;

typedef enum {
	BQ40Z50_CELL_1,
	BQ40Z50_CELL_2
} bq40z50_cell_t;

esp_err_t bq40z50_init(bq40z50_t *gauge, smbus_t *bus, int address);

esp_err_t bq40z50_get_battery_voltage_mv(bq40z50_t *gauge, unsigned int *res);
esp_err_t bq40z50_get_battery_temperature_mdegc(bq40z50_t *gauge, int32_t *res);
esp_err_t bq40z50_get_cell_voltage_mv(bq40z50_t *gauge, bq40z50_cell_t cell, unsigned int *res);
esp_err_t bq40z50_get_state_of_charge_percent(bq40z50_t *gauge, unsigned int *res);
esp_err_t bq40z50_get_state_of_health_percent(bq40z50_t *gauge, unsigned int *res);
esp_err_t bq40z50_get_full_charge_capacity_mah(bq40z50_t *gauge, unsigned int *res);
esp_err_t bq40z50_get_current_ma(bq40z50_t *gauge, int *res);
esp_err_t bq40z50_get_charging_current_ma(bq40z50_t *gauge, unsigned int *res);
esp_err_t bq40z50_get_charging_voltage_mv(bq40z50_t *gauge, unsigned int *res);
esp_err_t bq40z50_get_run_time_to_empty_min(bq40z50_t *gauge, unsigned int *res);
esp_err_t bq40z50_shutdown(bq40z50_t *gauge);
