#include "battery_protection.h"

#include "battery_gauge.h"
#include "power_path.h"
#include "scheduler.h"

#define MIN_CELL_VOLTAGE_MV 2800
#define UNDERVOLTAGE_SHUTDOWN_SECONDS 10

static bq40z50_t *gauge;
static unsigned int undervoltage_seconds = 0;
static scheduler_task_t gauge_poll_task;

static bool is_undervoltage(unsigned int cell_voltage_mv) {
	if (cell_voltage_mv == 0) {
		return false;
	}

	return cell_voltage_mv < MIN_CELL_VOLTAGE_MV;
}

static void gauge_poll_voltage_cb(void *ctx);
static void gauge_poll_voltage_cb(void *ctx) {
	unsigned int voltage_cell1_mv = battery_gauge_get_cell1_voltage_mv();
	unsigned int voltage_cell2_mv = battery_gauge_get_cell2_voltage_mv();

	if (power_path_is_running_on_battery() &&
	    (is_undervoltage(voltage_cell1_mv) || is_undervoltage(voltage_cell2_mv))) {
		undervoltage_seconds++;

		if (undervoltage_seconds >= UNDERVOLTAGE_SHUTDOWN_SECONDS) {
			bq40z50_shutdown(gauge);
			undervoltage_seconds = 0;
		}
	} else {
		undervoltage_seconds = 0;
	}

	scheduler_schedule_task_relative(&gauge_poll_task, gauge_poll_voltage_cb, NULL, MS_TO_US(1000));
}

void battery_protection_init(bq40z50_t *gauge_) {
	gauge = gauge_;
	scheduler_task_init(&gauge_poll_task);
	scheduler_schedule_task_relative(&gauge_poll_task, gauge_poll_voltage_cb, NULL, MS_TO_US(5000));
}

