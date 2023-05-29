#include "battery_gauge.h"

#include <errno.h>

#include <esp_log.h>

#include "event_bus.h"
#include "scheduler.h"
#include "util.h"

#define GAUGE_UPDATE_INTERVAL_MS	2000

static const char *TAG = "gauge";

static int32_t battery_params[BATTERY_PARAM_MAX_ + 1] = { 0 };
static battery_gauge_t *gauge;

static scheduler_task_t gauge_update_task;

static void gauge_update(void *ctx);
static void gauge_update(void *ctx) {
	battery_gauge_t *gauge = ctx;
	battery_param_t param;
	bool changed = false;

	for (param = BATTERY_VOLTAGE_MV; param < ARRAY_SIZE(battery_params); param++) {
		int err;
		int32_t val;

		err = gauge->ops->get_param(gauge, param, &val);
		if (err) {
			if (err == ENOTSUP) {
				ESP_LOGD(TAG, "Gauge does not support parameter %d", param);
			} else {
				ESP_LOGE(TAG, "Failed to get parameter %d from gauge: %d", param, err);
			}
		} else if (val != battery_params[param]) {
			battery_params[param] = val;
			changed = true;
		}
	}

	if (changed) {
		event_bus_notify("battery_gauge", NULL);
	}
	scheduler_schedule_task_relative(&gauge_update_task, gauge_update, gauge, MS_TO_US(GAUGE_UPDATE_INTERVAL_MS));
}

void battery_gauge_init(battery_gauge_t *gauge_) {
	gauge = gauge_;

	scheduler_task_init(&gauge_update_task);
	scheduler_schedule_task_relative(&gauge_update_task, gauge_update, gauge, 0);
}

unsigned int battery_gauge_get_soc_percent(void) {
	return CLAMP(battery_params[BATTERY_SOC_PERCENT], 0, 100);
}

unsigned int battery_gauge_get_soh_percent(void) {
	return CLAMP(battery_params[BATTERY_SOH_PERCENT], 0, 100);
}

long battery_gauge_get_current_ma(void) {
	return battery_params[BATTERY_CURRENT_MA];
}

unsigned int battery_gauge_get_time_to_empty_min(void) {
	return MAX(battery_params[BATTERY_TIME_TO_EMPTY_MIN], 0);
}

unsigned int battery_gauge_get_cell1_voltage_mv(void) {
	return CLAMP(battery_params[BATTERY_VOLTAGE_CELL1_MV], 0, 5000);
}

unsigned int battery_gauge_get_cell2_voltage_mv(void) {
	return CLAMP(battery_params[BATTERY_VOLTAGE_CELL2_MV], 0, 5000);
}

long battery_gauge_get_temperature_mdegc(void) {
	return battery_params[BATTERY_TEMPERATURE_MDEG_C];
}

unsigned int battery_gauge_get_full_charge_capacity_mah(void) {
	return battery_params[BATTERY_FULL_CHARGE_CAPACITY_MAH];
}
