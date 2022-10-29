#pragma once

#include <stdint.h>

#include <esp_err.h>

typedef struct temperature_sensor temperature_sensor_t;

typedef struct temperature_sensor_ops {
	esp_err_t (*read_temperature_mdegc)(temperature_sensor_t *sensor, int32_t *res);
} temperature_sensor_ops_t ;

struct temperature_sensor {
	const char *name;
	const temperature_sensor_ops_t *ops;
};

void temperature_sensor_init(temperature_sensor_t *sensor, const char *name, const temperature_sensor_ops_t *ops);
