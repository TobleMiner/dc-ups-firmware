#include "prometheus_metrics_temperature.h"

static unsigned int get_num_sensors(prometheus_temperature_metrics_t *temp_metric) {
	unsigned int num_sensors = 0;
	temperature_sensor_t **sensors = temp_metric->sensors;
	while (*(sensors++)) {
		num_sensors++;
	}

	return num_sensors;
}

static unsigned int get_num_values(prometheus_metric_t *metric) {
	prometheus_temperature_metrics_t *temp_metric = metric->priv;
	return get_num_sensors(temp_metric);
}

static unsigned int get_num_labels(const prometheus_metric_value_t *val, prometheus_metric_t *metric) {
	return 1;
}

static void get_label(const prometheus_metric_value_t *val, prometheus_metric_t *metric, unsigned int index, char *label, char *value) {
	strcpy(label, "sensor");
	temperature_sensor_t *sensor = val->priv;
	strcpy(value, sensor->name);
}

static void get_sensor_value(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	temperature_sensor_t *sensor = val->priv;
	int32_t temp_mdegc = 0;
	sensor->ops->read_temperature_mdegc(sensor, &temp_mdegc);
	sprintf(value, "%s%01u.%03u", temp_mdegc < 0 ? "-" : "", ABS(temp_mdegc) / 1000, ABS(temp_mdegc) % 1000);
}

static void get_value(prometheus_metric_t *metric, unsigned int index, prometheus_metric_value_t *value) {
	prometheus_temperature_metrics_t *temp_metric = metric->priv;
	value->priv = temp_metric->sensors[index];
	value->get_num_labels = get_num_labels;
	value->get_label = get_label;
	value->get_value = get_sensor_value;
}

const prometheus_metric_def_t temperature_metric_def = {
	.name = "temperature",
	.help = "System temperatures",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.num_values = 0,
	.get_num_values = get_num_values,
	.get_value = get_value,
};

void prometheus_temperature_metrics_init(prometheus_temperature_metrics_t *metrics, temperature_sensor_t **sensors) {
	prometheus_metric_init(&metrics->metric, &temperature_metric_def, metrics);
	metrics->sensors = sensors;
}

void prometheus_add_temperature_metrics(prometheus_temperature_metrics_t *metrics, prometheus_t *prometheus) {
	prometheus_add_metric(prometheus, &metrics->metric);
}
