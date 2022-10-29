#include "sensor.h"

#include <esp_log.h>

#include "util.h"

static const char *TAG = "sensor";

static DECLARE_LIST_HEAD(sensors);

static unsigned int get_num_values_by_type(sensor_measurement_type_t type) {
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

static unsigned int get_num_labels_by_type(const prometheus_metric_value_t *val, prometheus_metric_t *metric, sensor_measurement_type_t type) {
	unsigned int value_index = (unsigned int)val->priv;
	unsigned int channel;
	sensor_t *sensor = get_sensor_and_channel_by_type_and_index(type, value_index, &channel);
	if (sensor->def->get_channel_name && sensor->def->get_channel_name(sensor, type, channel)) {
		return 2;
	} else {
		return 1;
	}
}

static unsigned int get_num_labels_voltage(const prometheus_metric_value_t *val, prometheus_metric_t *metric) {
	return get_num_labels_by_type(val, metric, SENSOR_TYPE_VOLTAGE);
}

static unsigned int get_num_labels_current(const prometheus_metric_value_t *val, prometheus_metric_t *metric) {
	return get_num_labels_by_type(val, metric, SENSOR_TYPE_CURRENT);
}

static unsigned int get_num_labels_power(const prometheus_metric_value_t *val, prometheus_metric_t *metric) {
	return get_num_labels_by_type(val, metric, SENSOR_TYPE_POWER);
}

static unsigned int get_num_labels_temperature(const prometheus_metric_value_t *val, prometheus_metric_t *metric) {
	return get_num_labels_by_type(val, metric, SENSOR_TYPE_TEMPERATURE);
}

static void get_label_by_type(const prometheus_metric_value_t *val, prometheus_metric_t *metric, unsigned int index, char *label, char *value, sensor_measurement_type_t type) {
	unsigned int value_index = (unsigned int)val->priv;
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

static void get_label_voltage(const prometheus_metric_value_t *val, prometheus_metric_t *metric, unsigned int index, char *label, char *value) {
	get_label_by_type(val, metric, index, label, value, SENSOR_TYPE_VOLTAGE);
}

static void get_label_current(const prometheus_metric_value_t *val, prometheus_metric_t *metric, unsigned int index, char *label, char *value) {
	get_label_by_type(val, metric, index, label, value, SENSOR_TYPE_CURRENT);
}

static void get_label_power(const prometheus_metric_value_t *val, prometheus_metric_t *metric, unsigned int index, char *label, char *value) {
	get_label_by_type(val, metric, index, label, value, SENSOR_TYPE_POWER);
}

static void get_label_temperature(const prometheus_metric_value_t *val, prometheus_metric_t *metric, unsigned int index, char *label, char *value) {
	get_label_by_type(val, metric, index, label, value, SENSOR_TYPE_TEMPERATURE);
}

static void get_value_by_type(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value, sensor_measurement_type_t type) {
	unsigned int value_index = (unsigned int)val->priv;
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

static void get_value_voltage(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	return get_value_by_type(val, metric, value, SENSOR_TYPE_VOLTAGE);
}

static void get_value_current(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	return get_value_by_type(val, metric, value, SENSOR_TYPE_CURRENT);
}

static void get_value_power(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	return get_value_by_type(val, metric, value, SENSOR_TYPE_POWER);
}

static void get_value_temperature(const prometheus_metric_value_t *val, prometheus_metric_t *metric, char *value) {
	return get_value_by_type(val, metric, value, SENSOR_TYPE_TEMPERATURE);
}

static void get_metric_value_by_type(unsigned int index, prometheus_metric_value_t *value, sensor_measurement_type_t type) {
	value->priv = (void *)index;
	switch (type) {
	case SENSOR_TYPE_VOLTAGE:
		value->get_num_labels = get_num_labels_voltage;
		value->get_label = get_label_voltage;
		value->get_value = get_value_voltage;
		break;
	case SENSOR_TYPE_CURRENT:
		value->get_num_labels = get_num_labels_current;
		value->get_label = get_label_current;
		value->get_value = get_value_current;
		break;
	case SENSOR_TYPE_POWER:
		value->get_num_labels = get_num_labels_power;
		value->get_label = get_label_power;
		value->get_value = get_value_power;
		break;
	case SENSOR_TYPE_TEMPERATURE:
		value->get_num_labels = get_num_labels_temperature;
		value->get_label = get_label_temperature;
		value->get_value = get_value_temperature;
		break;
	}
}

static unsigned int get_num_values_voltage(prometheus_metric_t *metric) {
	return get_num_values_by_type(SENSOR_TYPE_VOLTAGE);
}

static void get_metric_value_voltage(prometheus_metric_t *metric, unsigned int index, prometheus_metric_value_t *value) {
	get_metric_value_by_type(index, value, SENSOR_TYPE_VOLTAGE);
}

static const prometheus_metric_def_t voltage_metric_def = {
	.name = "voltage",
	.help = "Voltage sensors",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.num_values = 0,
	.get_num_values = get_num_values_voltage,
	.get_value = get_metric_value_voltage,
};

static unsigned int get_num_values_current(prometheus_metric_t *metric) {
	return get_num_values_by_type(SENSOR_TYPE_CURRENT);
}

static void get_metric_value_current(prometheus_metric_t *metric, unsigned int index, prometheus_metric_value_t *value) {
	get_metric_value_by_type(index, value, SENSOR_TYPE_CURRENT);
}

static const prometheus_metric_def_t current_metric_def = {
	.name = "current",
	.help = "Current sensors",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.num_values = 0,
	.get_num_values = get_num_values_current,
	.get_value = get_metric_value_current,
};

static unsigned int get_num_values_power(prometheus_metric_t *metric) {
	return get_num_values_by_type(SENSOR_TYPE_POWER);
}

static void get_metric_value_power(prometheus_metric_t *metric, unsigned int index, prometheus_metric_value_t *value) {
	get_metric_value_by_type(index, value, SENSOR_TYPE_POWER);
}

static const prometheus_metric_def_t power_metric_def = {
	.name = "power",
	.help = "Power sensors",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.num_values = 0,
	.get_num_values = get_num_values_power,
	.get_value = get_metric_value_power,
};

static unsigned int get_num_values_temperature(prometheus_metric_t *metric) {
	return get_num_values_by_type(SENSOR_TYPE_TEMPERATURE);
}

static void get_metric_value_temperature(prometheus_metric_t *metric, unsigned int index, prometheus_metric_value_t *value) {
	get_metric_value_by_type(index, value, SENSOR_TYPE_TEMPERATURE);
}

static const prometheus_metric_def_t temperature_metric_def = {
	.name = "temperature",
	.help = "Temperature sensors",
	.type = PROMETHEUS_METRIC_TYPE_GAUGE,
	.num_values = 0,
	.get_num_values = get_num_values_temperature,
	.get_value = get_metric_value_temperature,
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
	prometheus_metric_init(&metric_voltage, &voltage_metric_def, NULL);
	prometheus_metric_init(&metric_current, &current_metric_def, NULL);
	prometheus_metric_init(&metric_power, &power_metric_def, NULL);
	prometheus_metric_init(&metric_temperature, &temperature_metric_def, NULL);
	prometheus_add_metric(prometheus, &metric_voltage);
	prometheus_add_metric(prometheus, &metric_current);
	prometheus_add_metric(prometheus, &metric_power);
	prometheus_add_metric(prometheus, &metric_temperature);
}

void sensor_add(sensor_t *sensor) {
	LIST_APPEND_TAIL(&sensor->list, &sensors);
}
