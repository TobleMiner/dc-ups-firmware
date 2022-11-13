#include "sensor.h"

#include <esp_log.h>

#include "util.h"

#define METRIC_PRIV(type_) ((void *)((unsigned int)(type_) & 0xf))
#define METRIC_PRIV_TYPE(priv_) ((sensor_measurement_type_t)((unsigned int)(priv_) & 0xf))

#define VALUE_PRIV(index_, type_) ((void *)(((unsigned int)(index_) << 4) | ((unsigned int)(type_) & 0xf)))
#define VALUE_PRIV_INDEX(priv_) ((unsigned int)(priv_) >> 4)
#define VALUE_PRIV_TYPE(priv_) ((sensor_measurement_type_t)((unsigned int)(priv_) & 0xf))

static const char *TAG = "sensor";

static DECLARE_LIST_HEAD(sensors);

static unsigned int get_num_values(prometheus_metric_t *metric) {
	sensor_measurement_type_t type = METRIC_PRIV_TYPE(metric->priv);
	sensor_t *sensor;
	unsigned int num_values = 0;
	LIST_FOR_EACH_ENTRY(sensor, &sensors, list) {
		num_values += sensor->def->get_num_channels(sensor, type);
	}
	return num_values;
}

static sensor_t *get_sensor_and_channel_by_type_and_index(sensor_measurement_type_t type, unsigned int index, unsigned int *channel) {
	sensor_t *sensor;
	LIST_FOR_EACH_ENTRY(sensor, &sensors, list) {
		unsigned int num_values = sensor->def->get_num_channels(sensor, type);
		if (num_values > index) {
			if (channel) {
				*channel = num_values - index - 1;
			}
			return sensor;
		}
		index -= num_values;
	}
	return NULL;
}

static unsigned int get_num_labels(const prometheus_metric_value_t *val, prometheus_metric_t *metric) {
	unsigned int value_index = VALUE_PRIV_INDEX(val->priv);
	sensor_measurement_type_t type = VALUE_PRIV_TYPE(val->priv);
	unsigned int channel;
	sensor_t *sensor = get_sensor_and_channel_by_type_and_index(type, value_index, &channel);
	if (sensor->def->get_channel_name && sensor->def->get_channel_name(sensor, type, channel)) {
		return 2;
	} else {
		return 1;
	}
}

static void get_label(const prometheus_metric_value_t *val, prometheus_metric_t *metric, unsigned int index, char *label, char *value) {
	unsigned int value_index = VALUE_PRIV_INDEX(val->priv);
	sensor_measurement_type_t type = VALUE_PRIV_TYPE(val->priv);
	unsigned int channel;
	sensor_t *sensor = get_sensor_and_channel_by_type_and_index(type, value_index, &channel);
	if (index == 0) {
		strcpy(label, "sensor");
		strcpy(value, sensor->name);
	} else {
		strcpy(label, "channel");
		strcpy(value, sensor->def->get_channel_name(sensor, type, channel));
	}
}

static void get_value(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	unsigned int value_index = VALUE_PRIV_INDEX(val->priv);
	sensor_measurement_type_t type = VALUE_PRIV_TYPE(val->priv);
	unsigned int channel;
	sensor_t *sensor = get_sensor_and_channel_by_type_and_index(type, value_index, &channel);
	long res;
	esp_err_t err = sensor->def->measure(sensor, type, channel, &res);
	if (err) {
		ESP_LOGE(TAG, "Failed to read %u channel %u of sensor %s", type, channel, sensor->name);
	} else {
		sprintf(value, "%s%01lu.%03lu", res < 0 ? "-" : "", ABS(res) / 1000, ABS(res) % 1000);
	}
}

static void get_metric_value(prometheus_metric_t *metric, unsigned int index, prometheus_metric_value_t *value) {
	sensor_measurement_type_t type = METRIC_PRIV_TYPE(metric->priv);
	value->priv = VALUE_PRIV(index, type);
	value->get_num_labels = get_num_labels;
	value->get_label = get_label;
	value->get_value = get_value;
}

static const prometheus_metric_def_t voltage_metric_def = {
	.name = "voltage",
	.help = "Voltage sensors",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.num_values = 0,
	.get_num_values = get_num_values,
	.get_value = get_metric_value,
};

static const prometheus_metric_def_t current_metric_def = {
	.name = "current",
	.help = "Current sensors",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.num_values = 0,
	.get_num_values = get_num_values,
	.get_value = get_metric_value,
};

static const prometheus_metric_def_t power_metric_def = {
	.name = "power",
	.help = "Power sensors",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.num_values = 0,
	.get_num_values = get_num_values,
	.get_value = get_metric_value,
};

static const prometheus_metric_def_t temperature_metric_def = {
	.name = "temperature",
	.help = "Temperature sensors",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.num_values = 0,
	.get_num_values = get_num_values,
	.get_value = get_metric_value,
};

static prometheus_metric_t metric_voltage;
static prometheus_metric_t metric_current;
static prometheus_metric_t metric_power;
static prometheus_metric_t metric_temperature;

void sensor_init(sensor_t *sensor, const sensor_def_t *def, const char *name) {
	INIT_LIST_HEAD(sensor->list);
	sensor->def = def;
	sensor->name = name;
}

void sensor_install_metrics(prometheus_t *prometheus) {
	prometheus_metric_init(&metric_voltage, &voltage_metric_def, METRIC_PRIV(SENSOR_TYPE_VOLTAGE));
	prometheus_metric_init(&metric_current, &current_metric_def, METRIC_PRIV(SENSOR_TYPE_CURRENT));
	prometheus_metric_init(&metric_power, &power_metric_def, METRIC_PRIV(SENSOR_TYPE_POWER));
	prometheus_metric_init(&metric_temperature, &temperature_metric_def, METRIC_PRIV(SENSOR_TYPE_TEMPERATURE));
	prometheus_add_metric(prometheus, &metric_voltage);
	prometheus_add_metric(prometheus, &metric_current);
	prometheus_add_metric(prometheus, &metric_power);
	prometheus_add_metric(prometheus, &metric_temperature);
}

void sensor_add(sensor_t *sensor) {
	LIST_APPEND_TAIL(&sensor->list, &sensors);
}

sensor_t *sensor_find_by_name(const char *name) {
	sensor_t *sensor;
	LIST_FOR_EACH_ENTRY(sensor, &sensors, list) {
		if (strcmp(sensor->name, name) == 0) {
			return sensor;
		}
	}

	return NULL;
}
