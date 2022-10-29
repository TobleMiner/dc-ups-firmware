#include "temperature_sensor.h"

void temperature_sensor_init(temperature_sensor_t *sensor, const char *name, const temperature_sensor_ops_t *ops) {
	sensor->name = name;
	sensor->ops = ops;
}