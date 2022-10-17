#pragma once

#include <esp_err.h>

#include "i2c_bus.h"

typedef struct ina219 {
	i2c_bus_t *bus;
	unsigned int address;
	unsigned int shunt_resistance_mohms;
} ina219_t;

typedef enum {
	INA219_BUS_VOLTAGE_RANGE_16V,
	INA219_BUS_VOLTAGE_RANGE_32V
} ina219_bus_voltage_range_t;

typedef enum {
	INA219_PGA_CURRENT_GAIN_40MV = 0,
	INA219_PGA_CURRENT_GAIN_80MV = 1,
	INA219_PGA_CURRENT_GAIN_160MV = 2,
	INA219_PGA_CURRENT_GAIN_320MV = 3
} ina219_pga_current_gain_t;

typedef enum {
	INA219_ADC_RESOLUTION_9BIT = 0,
	INA219_ADC_RESOLUTION_10BIT = 1,
	INA219_ADC_RESOLUTION_11BIT = 2,
	INA219_ADC_RESOLUTION_12BIT = 3,
	INA219_ADC_RESOLUTION_AVG_2 = 9,
	INA219_ADC_RESOLUTION_AVG_4 = 10,
	INA219_ADC_RESOLUTION_AVG_8 = 11,
	INA219_ADC_RESOLUTION_AVG_16 = 12,
	INA219_ADC_RESOLUTION_AVG_32 = 13,
	INA219_ADC_RESOLUTION_AVG_64 = 14,
	INA219_ADC_RESOLUTION_AVG_128 = 15,
} ina219_adc_resolution_t;

esp_err_t ina219_init(ina219_t *ina, i2c_bus_t *bus, unsigned int address, unsigned int shunt_resistance_mohms);
esp_err_t ina219_reset(ina219_t *ina);
esp_err_t ina219_set_voltage_range(ina219_t *ina, ina219_bus_voltage_range_t range);
esp_err_t ina219_set_shunt_voltage_range(ina219_t *ina, ina219_pga_current_gain_t gain);
esp_err_t ina219_set_shunt_voltage_resolution(ina219_t *ina, ina219_adc_resolution_t resolution);
esp_err_t ina219_set_bus_voltage_adc_resolution(ina219_t *ina, ina219_adc_resolution_t resolution);
esp_err_t ina219_read_shunt_voltage_uv(ina219_t *ina, long *shunt_voltage_uv);
esp_err_t ina219_read_bus_voltage_mv(ina219_t *ina, unsigned int *bus_voltage_mv);
esp_err_t ina219_read_current_ua(ina219_t *ina, long *current_ua);
esp_err_t ina219_read_power_uw(ina219_t *ina, long *power_uw);
