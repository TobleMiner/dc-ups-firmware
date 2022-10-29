#pragma once

#include "prometheus.h"
#include "temperature_sensor.h"

typedef struct prometheus_temperature_metrics {
	prometheus_metric_t metric;
	temperature_sensor_t **sensors;
} prometheus_temperature_metrics_t;

void prometheus_temperature_metrics_init(prometheus_temperature_metrics_t *metrics, temperature_sensor_t **sensors);
void prometheus_add_temperature_metrics(prometheus_temperature_metrics_t *metrics, prometheus_t *prometheus);
