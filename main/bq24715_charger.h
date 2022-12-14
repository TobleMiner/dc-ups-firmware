#pragma once

#include "smbus.h"

typedef struct bq24715 {
	smbus_t *bus;
} bq24715_t;

esp_err_t bq24715_init(bq24715_t *charger, smbus_t *smbus);
esp_err_t bq24715_set_charge_current(bq24715_t *charger, unsigned int current_ma);
esp_err_t bq24715_set_max_charge_voltage(bq24715_t *charger, unsigned int voltage_mv);
esp_err_t bq24715_set_min_system_voltage(bq24715_t *charger, unsigned int voltage_mv);
esp_err_t bq24715_set_input_current(bq24715_t *charger, unsigned int current_ma);
