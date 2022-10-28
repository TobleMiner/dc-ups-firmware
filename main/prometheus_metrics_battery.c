#include "prometheus_metrics_battery.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "util.h"

static void get_pack_voltage(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	bq40z50_t *gauge = metric->priv;
	unsigned int voltage_mv = 0;
	bq40z50_get_battery_voltage_mv(gauge, &voltage_mv);
	sprintf(value, "%01u.%03u", voltage_mv / 1000, voltage_mv % 1000);
}

static void get_cell_voltage(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value, bq40z50_cell_t cell) {
	bq40z50_t *gauge = metric->priv;
	unsigned int voltage_mv = 0;
	bq40z50_get_cell_voltage_mv(gauge, cell, &voltage_mv);
	sprintf(value, "%01u.%03u", voltage_mv / 1000, voltage_mv % 1000);
}

static void get_cell_label(const prometheus_metric_value_t *val, prometheus_metric_t *metric, unsigned int index, char *label, char *value, unsigned int cell) {
	strcpy(label, "cell");
	sprintf(value, "%01u", cell);
}

static unsigned int get_num_labels_cell_voltage(const prometheus_metric_value_t *val, prometheus_metric_t *metric) {
	return 1;
}

static void get_cell1_label(const prometheus_metric_value_t *val, prometheus_metric_t *metric, unsigned int index, char *label, char *value) {
	get_cell_label(val, metric, index, label, value, 1);
}

static void get_cell2_label(const prometheus_metric_value_t *val, prometheus_metric_t *metric, unsigned int index, char *label, char *value) {
	get_cell_label(val, metric, index, label, value, 2);
}

static void get_cell1_voltage(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	get_cell_voltage(val, metric, value, BQ40Z50_CELL_1);
}

static void get_cell2_voltage(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	get_cell_voltage(val, metric, value, BQ40Z50_CELL_2);
}

static const prometheus_metric_value_t battery_voltage_values[] = {
	{
		.get_num_labels = NULL,
		.get_value = get_pack_voltage,
	}, {
		.get_num_labels = get_num_labels_cell_voltage,
		.get_label = get_cell1_label,
		.get_value = get_cell1_voltage,
	}, {
		.get_num_labels = get_num_labels_cell_voltage,
		.get_label = get_cell2_label,
		.get_value = get_cell2_voltage,
	}
};

static const prometheus_metric_def_t battery_voltage_metric_def = {
	.name = "battery_voltage",
	.help = "UPS battery voltage",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.values = battery_voltage_values,
	.num_values = ARRAY_SIZE(battery_voltage_values),
};



static void get_battery_current(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	bq40z50_t *gauge = metric->priv;
	int current_ma = 0;
	bq40z50_get_current_ma(gauge, &current_ma);
	sprintf(value, "%s%01u.%03u", current_ma < 0 ? "-" : "", ABS(current_ma) / 1000, ABS(current_ma) % 1000);
}

static const prometheus_metric_value_t battery_current_value = {
	.get_num_labels = NULL,
	.get_value = get_battery_current,
};

static const prometheus_metric_def_t battery_current_metric_def = {
	.name = "battery_current",
	.help = "UPS battery current",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.values = &battery_current_value,
	.num_values = 1,
};



static void get_state_of_charge(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	bq40z50_t *gauge = metric->priv;
	unsigned int soc = 0;
	bq40z50_get_state_of_charge_percent(gauge, &soc);
	sprintf(value, "%u", soc);
}

static const prometheus_metric_value_t battery_state_of_charge_value = {
	.get_num_labels = NULL,
	.get_value = get_state_of_charge,
};

static const prometheus_metric_def_t battery_state_of_charge_metric_def = {
	.name = "battery_state_of_charge",
	.help = "UPS battery state of charge in percent",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.values = &battery_state_of_charge_value,
	.num_values = 1,
};

void prometheus_battery_metrics_init(prometheus_battery_metrics_t *metrics, bq40z50_t *gauge) {
	prometheus_metric_init(&metrics->voltage, &battery_voltage_metric_def, gauge);
	prometheus_metric_init(&metrics->current, &battery_current_metric_def, gauge);
	prometheus_metric_init(&metrics->state_of_charge, &battery_state_of_charge_metric_def, gauge);
}

void prometheus_add_battery_metrics(prometheus_battery_metrics_t *metrics, prometheus_t *prometheus) {
	prometheus_add_metric(prometheus, &metrics->voltage);
	prometheus_add_metric(prometheus, &metrics->current);
	prometheus_add_metric(prometheus, &metrics->state_of_charge);
}