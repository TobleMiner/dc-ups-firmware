#include "sensor.h"

#include <esp_log.h>

#include "callcache.h"
#include "util.h"

#define METRIC_PRIV(type_) ((void *)((unsigned int)(type_) & 0xf))
#define METRIC_PRIV_TYPE(priv_) ((sensor_measurement_type_t)((unsigned int)(priv_) & 0xf))

#define VALUE_PRIV(index_, type_) ((void *)(((unsigned int)(index_) << 4) | ((unsigned int)(type_) & 0xf)))
#define VALUE_PRIV_INDEX(priv_) ((unsigned int)(priv_) >> 4)
#define VALUE_PRIV_TYPE(priv_) ((sensor_measurement_type_t)((unsigned int)(priv_) & 0xf))

static const char *TAG = "sensor";

typedef struct sensor_subsystem {
	list_head_t sensors;
	message_bus_t *message_bus;
	callcache_t callcache;
} sensor_subsystem_t;

static sensor_subsystem_t subsystem;

void sensor_subsystem_init(message_bus_t *message_bus) {
	INIT_LIST_HEAD(subsystem.sensors);
	subsystem.message_bus = message_bus;
	callcache_init(&subsystem.callcache);
}

static unsigned int get_num_values(prometheus_metric_t *metric) {
	sensor_measurement_type_t type = METRIC_PRIV_TYPE(metric->priv);
	sensor_t *sensor;
	unsigned int num_values = 0;
	LIST_FOR_EACH_ENTRY(sensor, &subsystem.sensors, list) {
		num_values += sensor->def->get_num_channels(sensor, type);
	}
	return num_values;
}

static sensor_t *get_sensor_and_channel_by_type_and_index(sensor_measurement_type_t type, unsigned int index, unsigned int *channel) {
	sensor_t *sensor;
	LIST_FOR_EACH_ENTRY(sensor, &subsystem.sensors, list) {
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
	esp_err_t err = sensor_measure(sensor, type, channel, &res);
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
	sensor->lock = xSemaphoreCreateMutexStatic(&sensor->lock_buffer);
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
	LIST_APPEND_TAIL(&sensor->list, &subsystem.sensors);
}

sensor_t *sensor_find_by_name(const char *name) {
	sensor_t *sensor;
	LIST_FOR_EACH_ENTRY(sensor, &subsystem.sensors, list) {
		if (strcmp(sensor->name, name) == 0) {
			return sensor;
		}
	}

	return NULL;
}

static esp_err_t measure_cacheable(callcache_call_arg_t *args, unsigned int num_args,
				   callcache_call_arg_t *return_values, unsigned int *num_return_val,
				   int64_t *cache_ttl_us) {
	if (num_args != 3) {
		return ESP_ERR_INVALID_ARG;
	}

	callcache_call_arg_t *sensor_arg = &args[0];
	callcache_call_arg_t *type_arg = &args[1];
	callcache_call_arg_t *channel_arg = &args[2];

	if (sensor_arg->type != CALLCACHE_CALL_ARG_TYPE_PTR) {
		return ESP_ERR_INVALID_ARG;
	}
	if (type_arg->type != CALLCACHE_CALL_ARG_TYPE_UINT) {
		return ESP_ERR_INVALID_ARG;
	}
	if (channel_arg->type != CALLCACHE_CALL_ARG_TYPE_UINT) {
		return ESP_ERR_INVALID_ARG;
	}

	sensor_t *sensor = sensor_arg->value.ptr_val;
	sensor_measurement_type_t type = type_arg->value.uint_val;
	unsigned int channel = channel_arg->value.uint_val;

	if (*num_return_val >= 1) {
		long res;
		esp_err_t err = sensor->def->measure(sensor, type, channel, &res);
		if (err) {
			return err;
		}
		return_values->type = CALLCACHE_CALL_ARG_TYPE_INT;
		return_values->value.int_val = res;
		*num_return_val = 1;
		*cache_ttl_us = 10LL * 1000LL * 1000LL;
	}

	return ESP_OK;
}

static callcache_callee_t callcache_sensor_measure = {
	.call = measure_cacheable,
	.is_call_arg_cacheable = NULL
};

void notify_message_bus(sensor_t *sensor, sensor_measurement_type_t type, unsigned int channel) {
	sensor_update_message_t update_msg = {
		.sensor = sensor,
		.type = type,
		.channel = channel
	};
	message_bus_broadcast(subsystem.message_bus,
			      MESSAGE_BUS_MESSAGE_TYPE_SENSOR_MEASUREMENT_UPDATE,
			      &update_msg);
}

esp_err_t sensor_measure(sensor_t *sensor, sensor_measurement_type_t type, unsigned int index, long *res) {
	callcache_call_arg_t call_args[] = {
		{
			.type = CALLCACHE_CALL_ARG_TYPE_PTR,
			.value.ptr_val = sensor
		},
		{
			.type = CALLCACHE_CALL_ARG_TYPE_UINT,
			.value.uint_val = type
		},
		{
			.type = CALLCACHE_CALL_ARG_TYPE_UINT,
			.value.uint_val = index
		},

	};
	callcache_call_arg_t return_values[1];
	unsigned int num_return_values = 1;
	bool changed;
	xSemaphoreTake(sensor->lock, portMAX_DELAY);
	esp_err_t err = callcache_call_changed(&subsystem.callcache, &callcache_sensor_measure,
					       call_args, ARRAY_SIZE(call_args),
					       return_values, &num_return_values,
					       &changed);
	if (err) {
		return err;
	}
	if (changed && subsystem.message_bus) {
		notify_message_bus(sensor, type, index);
	}
	xSemaphoreGive(sensor->lock);
	if (res) {
		*res = return_values->value.int_val;
	}
	return ESP_OK;
}

esp_err_t sensor_subsystem_update_all_sensors(void) {
	esp_err_t err = ESP_OK;

	sensor_t *sensor;
	LIST_FOR_EACH_ENTRY(sensor, &subsystem.sensors, list) {
		if (!sensor->def->get_num_channels) {
			continue;
		}
		for (sensor_measurement_type_t type = 0; type < SENSOR_TYPE_MAX; type++) {
			unsigned int num_channels = sensor->def->get_num_channels(sensor, type);
			for (unsigned int channel = 0; channel < num_channels; channel++) {
				esp_err_t sensor_err = sensor_measure(sensor, type, channel, NULL);
				if (sensor_err) {
					ESP_LOGE(TAG, "Failed to update sensor %s: %d (0x%03x)", sensor->name, sensor_err, sensor_err);
					if (!err) {
						err = sensor_err;
					}
				}
			}
		}
	}
	return err;
}
