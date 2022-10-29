#pragma once

#include "bq40z50_gauge.h"
#include "prometheus.h"

typedef struct prometheus_battery_metrics {
	prometheus_metric_t state_of_charge;
} prometheus_battery_metrics_t;

void prometheus_battery_metrics_init(prometheus_battery_metrics_t *metrics, bq40z50_t *gauge);
void prometheus_add_battery_metrics(prometheus_battery_metrics_t *metrics, prometheus_t *prometheus);
