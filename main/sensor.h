#pragma once

#include <esp_err.h>

#include "list.h"
#include "prometheus.h"

#define SENSOR_CHANNEL_NAME_LEN	64

typedef enum {
	SENSOR_TYPE_VOLTAGE	= 0,
	SENSOR_TYPE_CURRENT	= 1,
	SENSOR_TYPE_POWER	= 2,
	SENSOR_TYPE_TEMPERATURE	= 3,
} sensor_measurement_type_t;

typedef struct sensor sensor_t;

typedef struct sensor_def {
	unsigned int (*get_num_channels)(sensor_t *sensor, sensor_measurement_type_t type);
	const char *(*get_channel_name)(sensor_t *sensor, sensor_measurement_type_t type, unsigned int index);
	esp_err_t (*measure)(sensor_t *sensor, sensor_measurement_type_t type, unsigned int index, long *res);
} sensor_def_t;

struct sensor {
	const sensor_def_t *def;
	const char *name;
	list_head_t list;
};

void sensor_init(sensor_t *sensor, const sensor_def_t *def, const char *name);
void sensor_install_metrics(prometheus_t *prometheus);
void sensor_add(sensor_t *sensor);