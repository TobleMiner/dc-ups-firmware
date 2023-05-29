#include "power_path.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>

#include "battery_gauge.h"
#include "bq24715_charger.h"
#include "event_bus.h"
#include "ina219.h"
#include "lm75.h"
#include "scheduler.h"
#include "settings.h"
#include "util.h"

#define GPIO_DCOK	34

#define GPIO_VSEL0	35
#define GPIO_VSEL1	36

#define POWER_UPDATE_INTERVAL_MS	1000

#define BATTERY_CHARGE_VOLTAGE_MV	8400
#define BATTERY_NOMINAL_VOLTAGE_MV	7400
#define DEFAULT_CHARGE_CURRENT_MA	256
#define MAX_CHARGE_CURRENT_MA		1024
#define MAX_INPUT_CURRENT_MA		4000

typedef enum ina_type {
	INA_TYPE_DC_IN			= 0,
	INA_TYPE_DC_OUT_PASSTHROUGH	= 1,
	INA_TYPE_DC_OUT_STEP_UP		= 2,
	INA_TYPE_USB_OUT		= 3
} ina_type_t;

typedef struct ina_def {
	const char *name;
	unsigned int address;
	unsigned int shunt_resistance_mohms;
} ina_def_t;

typedef struct ina_state {
	ina219_t ina;
	unsigned int voltage_mv;
	long current_ua;
} ina_state_t;

typedef struct lm75_def {
	const char *name;
	unsigned int address;
} lm75_def_t;

typedef struct lm75_state {
	lm75_t lm75;
	int32_t temperature_mdegc;
} lm75_state_t;

static const char *TAG = "power_path";

static const ina_def_t ina_defs[] = {
	{ "ina_dc_in",			0x40, 10 },
	{ "ina_dc_out_passthrough",	0x41, 10 },
	{ "ina_dc_out_step_up",		0x42, 10 },
	{ "ina_usb_out",		0x43, 10 },
};

static ina_state_t inas[ARRAY_SIZE(ina_defs)] = { 0 };

static const lm75_def_t lm75_defs[] = {
	{ "lm75_charger",	0x48 },
	{ "lm75_dc_out",	0x49 },
	{ "lm75_usb_out",	0x4a },
};

static lm75_state_t lm75s[ARRAY_SIZE(lm75_defs)] = { 0 };

static scheduler_task_t power_path_update_task;

static unsigned int input_current_limit_ma = 0;

static bq24715_t bq24715;

static unsigned long output_power_mw = 0;
static bool running_on_battery = false;

static power_path_group_data_t group_data[POWER_PATH_GROUP_MAX_ + 1] = { 0 };

static const unsigned int dc_output_voltage_table[] = {
	9000,
	12000,
	12500,
	15000
};

static bool dc_output_enable_table[] = {
	true,
	true,
	true
};

static void update_ina(ina_state_t *ina_state) {
	ina219_t *ina = &ina_state->ina;

	ina219_read_bus_voltage_mv(ina, &ina_state->voltage_mv);
	ina219_read_current_ua(ina, &ina_state->current_ua);
}

static long long calculate_output_power_uw(void) {
	int i;
	long long output_power_uw = 0;
	ina_type_t types_out[] = {
		INA_TYPE_DC_OUT_PASSTHROUGH,
		INA_TYPE_DC_OUT_STEP_UP,
		INA_TYPE_USB_OUT
	};

	for (i = 0; i < ARRAY_SIZE(types_out); i++) {
		ina_state_t *ina_state = &inas[types_out[i]];
		long long power_uw =
			DIV_ROUND((long long)ina_state->voltage_mv * (long long)ina_state->current_ua, 1000);
		output_power_uw += power_uw;
	}

	return output_power_uw;
}

static long long calculate_max_input_power_uw(void) {
	ina_state_t *ina_dc_in_state = &inas[INA_TYPE_DC_IN];
	long long max_power_uw = ina_dc_in_state->voltage_mv * input_current_limit_ma;

	return max_power_uw;
}

static void update_charge_current(void) {
	long long current_output_power_uw = calculate_output_power_uw();
	long long max_input_power_uw = calculate_max_input_power_uw();
	unsigned int charge_current_setpoint_ma = 0;
	long predicted_discharge_current_ma = current_output_power_uw / BATTERY_NOMINAL_VOLTAGE_MV;

	battery_gauge_set_at_rate(-predicted_discharge_current_ma);

	output_power_mw = DIV_ROUND(current_output_power_uw, 1000);

	ESP_LOGI(TAG, "Output power: %ldmW", (long)DIV_ROUND(current_output_power_uw, 1000));
	ESP_LOGI(TAG, "Max input power: %ldmW", (long)DIV_ROUND(max_input_power_uw, 1000));
	if (current_output_power_uw < max_input_power_uw) {
		long long charging_power_uw = max_input_power_uw - current_output_power_uw;
		long charging_current_ma = charging_power_uw / BATTERY_CHARGE_VOLTAGE_MV;

		ESP_LOGI(TAG, "Charging power budget: %ldmW", (long)DIV_ROUND(charging_power_uw, 1000));

		if (charging_current_ma > MAX_CHARGE_CURRENT_MA) {
			charging_current_ma = MAX_CHARGE_CURRENT_MA;
		}
		charge_current_setpoint_ma = charging_current_ma;
	}
	ESP_LOGI(TAG, "Charge current setpoint: %umA", charge_current_setpoint_ma);

	bq24715_set_charge_current(&bq24715, charge_current_setpoint_ma);
	bq24715_set_input_current(&bq24715, input_current_limit_ma);
}

static void power_path_group_add_ina_data_(power_path_group_data_t *data, const ina_state_t *ina) {
	unsigned long current_ma = DIV_ROUND(ina->current_ua, 1000);
	unsigned long power_uw = ina->voltage_mv * current_ma;
	unsigned long power_mw = DIV_ROUND(power_uw, 1000);

	data->voltage_mv = ina->voltage_mv;
	data->current_ma += current_ma;
	data->power_mw += power_mw;
}

static void power_path_group_set_ina_data(power_path_group_data_t *data, const ina_state_t *ina, const ina_state_t *ina2) {
	power_path_group_data_t data_tmp = { 0 };

	power_path_group_add_ina_data_(&data_tmp, ina);
	if (ina2) {
		power_path_group_add_ina_data_(&data_tmp, ina2);
	}

	*data = data_tmp;
}

static void power_path_update_group_data(void) {
	int i;

	for (i = 0; i < ARRAY_SIZE(ina_defs); i++) {
		ina_state_t *ina_state = &inas[i];
		const ina_def_t *ina_def = &ina_defs[i];

		update_ina(ina_state);
	}

	power_path_group_set_ina_data(&group_data[POWER_PATH_GROUP_IN],
				      &inas[INA_TYPE_DC_IN], NULL);
	power_path_group_set_ina_data(&group_data[POWER_PATH_GROUP_DC],
				      &inas[INA_TYPE_DC_OUT_PASSTHROUGH],
				      &inas[INA_TYPE_DC_OUT_STEP_UP]);
	power_path_group_set_ina_data(&group_data[POWER_PATH_GROUP_USB],
				      &inas[INA_TYPE_USB_OUT], NULL);

	for (i = 0; i < ARRAY_SIZE(lm75_defs); i++) {
		lm75_state_t *lm75_state = &lm75s[i];
		const lm75_def_t *lm75_def = &lm75_defs[i];

		lm75_read_temperature_mdegc(&lm75_state->lm75, &group_data[i].temperature_mdegc);
	}

}

static void power_path_update_cb(void *ctx);
static void power_path_update_cb(void *ctx) {
	bool on_battery = power_path_is_running_on_battery();

	power_path_update_group_data();

	update_charge_current();

	if (running_on_battery != on_battery) {
		running_on_battery = on_battery;
		event_bus_notify("power_source", NULL);
	}

	event_bus_notify("power_path", NULL);
	scheduler_schedule_task_relative(&power_path_update_task, power_path_update_cb, NULL, MS_TO_US(POWER_UPDATE_INTERVAL_MS));
}

static void power_path_set_input_current_limit_(unsigned int current_ma) {
	if (current_ma > MAX_INPUT_CURRENT_MA) {
		current_ma = MAX_INPUT_CURRENT_MA;
	}
	input_current_limit_ma = current_ma;
}

void power_path_init(smbus_t *smbus, i2c_bus_t *i2c_bus) {
	int i;

	for (i = 0; i < ARRAY_SIZE(ina_defs); i++) {
		ina219_t *ina = &inas[i].ina;
		const ina_def_t *ina_def = &ina_defs[i];

		ESP_ERROR_CHECK(ina219_init(ina, smbus, ina_def->address, ina_def->shunt_resistance_mohms, ina_def->name));
		ESP_ERROR_CHECK(ina219_set_shunt_voltage_range(ina, INA219_PGA_CURRENT_GAIN_80MV));
	}

	for (i = 0; i < ARRAY_SIZE(lm75_defs); i++) {
		lm75_init(&lm75s[i].lm75, i2c_bus, lm75_defs[i].address, lm75_defs[i].name);
	}

	ESP_ERROR_CHECK(bq24715_init(&bq24715, smbus));
	ESP_ERROR_CHECK(bq24715_set_max_charge_voltage(&bq24715, BATTERY_CHARGE_VOLTAGE_MV));
	ESP_ERROR_CHECK(bq24715_set_charge_current(&bq24715, DEFAULT_CHARGE_CURRENT_MA));

	power_path_set_input_current_limit_(settings_get_input_current_limit_ma());

	scheduler_task_init(&power_path_update_task);
	scheduler_schedule_task_relative(&power_path_update_task, power_path_update_cb, NULL, 0);
}

void power_path_set_input_current_limit(unsigned int current_ma) {
	power_path_set_input_current_limit_(current_ma);
	settings_set_input_current_limit_ma(current_ma);
}

unsigned int power_path_get_input_current_limit_ma(void) {
	return input_current_limit_ma;
}


bool power_path_is_running_on_battery() {
	gpio_set_direction(GPIO_DCOK, GPIO_MODE_INPUT);
	return !gpio_get_level(GPIO_DCOK);
}

unsigned int power_path_get_dc_output_voltage_mv() {
	gpio_set_direction(GPIO_VSEL0, GPIO_MODE_INPUT);
	gpio_set_direction(GPIO_VSEL1, GPIO_MODE_INPUT);

	unsigned int lookup_idx =
		(gpio_get_level(GPIO_VSEL0) ? 1 : 0) |
		((gpio_get_level(GPIO_VSEL1) ? 1 : 0) << 1);
	return dc_output_voltage_table[lookup_idx];
}

bool power_path_is_dc_output_enabled(unsigned int output_idx) {
	return dc_output_enable_table[output_idx];
}

unsigned long power_path_get_output_power_consumption_mw() {
	return output_power_mw;
}

void power_path_get_group_data(power_path_group_t group, power_path_group_data_t *data) {
	*data = group_data[group];
}
