#include "prometheus_metrics_battery.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "util.h"

static void get_state_of_charge(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	bq40z50_t *gauge = metric->priv;
	unsigned int soc = 0;
	bq40z50_get_state_of_charge_percent(gauge, &soc);
	sprintf(value, "%u", soc);
}

static const prometheus_metric_value_t battery_state_of_charge_value = {
	.num_labels = 0,
	.get_num_labels = NULL,
	.get_value = get_state_of_charge,
};

static const prometheus_metric_def_t battery_state_of_charge_metric_def = {
	.name = "battery_state_of_charge",
	.help = "UPS battery state of charge in percent",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.values = &battery_state_of_charge_value,
	.num_values = 1,
	.get_num_values = NULL,
};

static void get_state_of_health(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	bq40z50_t *gauge = metric->priv;
	unsigned int soh = 0;
	bq40z50_get_state_of_health_percent(gauge, &soh);
	sprintf(value, "%u", soh);
}

static const prometheus_metric_value_t battery_state_of_health_value = {
	.num_labels = 0,
	.get_num_labels = NULL,
	.get_value = get_state_of_health,
};

static const prometheus_metric_def_t battery_state_of_health_metric_def = {
	.name = "battery_state_of_health",
	.help = "UPS battery state of health in percent",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.values = &battery_state_of_health_value,
	.num_values = 1,
	.get_num_values = NULL,
};

static void get_full_charge_capacity(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	bq40z50_t *gauge = metric->priv;
	unsigned int fcc = 0;
	bq40z50_get_full_charge_capacity_mah(gauge, &fcc);
	sprintf(value, "%f", fcc / 1000.f);
}

static const prometheus_metric_value_t battery_full_charge_capacity_value = {
	.num_labels = 0,
	.get_num_labels = NULL,
	.get_value = get_full_charge_capacity,
};

static const prometheus_metric_def_t battery_full_charge_capacity_metric_def = {
	.name = "battery_full_charge_capacity",
	.help = "UPS battery full charge capacity in Ah",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.values = &battery_full_charge_capacity_value,
	.num_values = 1,
	.get_num_values = NULL,
};

void prometheus_battery_metrics_init(prometheus_battery_metrics_t *metrics, bq40z50_t *gauge) {
	prometheus_metric_init(&metrics->state_of_charge, &battery_state_of_charge_metric_def, gauge);
	prometheus_metric_init(&metrics->state_of_health, &battery_state_of_health_metric_def, gauge);
	prometheus_metric_init(&metrics->full_charge_capacity, &battery_full_charge_capacity_metric_def, gauge);
}

void prometheus_add_battery_metrics(prometheus_battery_metrics_t *metrics, prometheus_t *prometheus) {
	prometheus_add_metric(prometheus, &metrics->state_of_charge);
	prometheus_add_metric(prometheus, &metrics->state_of_health);
	prometheus_add_metric(prometheus, &metrics->full_charge_capacity);
}
