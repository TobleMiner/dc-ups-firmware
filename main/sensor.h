#pragma once

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "list.h"
#include "message_bus.h"
#include "prometheus.h"

#define SENSOR_CHANNEL_NAME_LEN	64

typedef enum {
	SENSOR_TYPE_VOLTAGE	= 0,
	SENSOR_TYPE_CURRENT	= 1,
	SENSOR_TYPE_POWER	= 2,
	SENSOR_TYPE_TEMPERATURE	= 3,
	SENSOR_TYPE_MAX		= SENSOR_TYPE_TEMPERATURE
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
	StaticSemaphore_t lock_buffer;
	SemaphoreHandle_t lock;
};

typedef struct sensor_update_message {
	sensor_t *sensor;
	sensor_measurement_type_t type;
	unsigned int channel;
} sensor_update_message_t;

void sensor_subsystem_init(message_bus_t *message_bus);
esp_err_t sensor_subsystem_update_all_sensors(void);
void sensor_init(sensor_t *sensor, const sensor_def_t *def, const char *name);
void sensor_install_metrics(prometheus_t *prometheus);
void sensor_add(sensor_t *sensor);
sensor_t *sensor_find_by_name(const char *name);
esp_err_t sensor_measure(sensor_t *sensor, sensor_measurement_type_t type, unsigned int index, long *res);
esp_err_t sensor_subsystem_update_all_sensors(void);

static inline esp_err_t sensor_measure_voltage(sensor_t *sensor, unsigned int index, long *res) {
	return sensor_measure(sensor, SENSOR_TYPE_VOLTAGE, index, res);
}

static inline esp_err_t sensor_measure_current(sensor_t *sensor, unsigned int index, long *res) {
	return sensor_measure(sensor, SENSOR_TYPE_CURRENT, index, res);
}

static inline esp_err_t sensor_measure_power(sensor_t *sensor, unsigned int index, long *res) {
	return sensor_measure(sensor, SENSOR_TYPE_POWER, index, res);
}

static inline esp_err_t sensor_measure_temperature(sensor_t *sensor, unsigned int index, long *res) {
	return sensor_measure(sensor, SENSOR_TYPE_TEMPERATURE, index, res);
}
