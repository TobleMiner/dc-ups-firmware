#pragma once

#include <stdint.h>

typedef enum battery_param {
	BATTERY_VOLTAGE_MV,
	BATTERY_VOLTAGE_CELL1_MV,
	BATTERY_VOLTAGE_CELL2_MV,
	BATTERY_SOC_PERCENT,
	BATTERY_SOH_PERCENT,
	BATTERY_CURRENT_MA,
	BATTERY_TIME_TO_EMPTY_MIN,
	BATTERY_AT_RATE_TIME_TO_EMPTY_MIN,
	BATTERY_RATE_MA,
	BATTERY_TEMPERATURE_MDEG_C,
	BATTERY_FULL_CHARGE_CAPACITY_MAH,
	BATTERY_PARAM_MAX_ = BATTERY_FULL_CHARGE_CAPACITY_MAH
} battery_param_t;


typedef struct battery_gauge battery_gauge_t;
typedef struct battery_gauge_ops {
	int (*get_param)(battery_gauge_t *gauge, battery_param_t param, int32_t *retval);
	int (*set_param)(battery_gauge_t *gauge, battery_param_t param, int32_t val);
} battery_gauge_ops_t;


struct battery_gauge {
	void *priv;
	const battery_gauge_ops_t *ops;
};

void battery_gauge_init(battery_gauge_t *gauge);

unsigned int battery_gauge_get_soc_percent(void);
unsigned int battery_gauge_get_soh_percent(void);
long battery_gauge_get_current_ma(void);
unsigned int battery_gauge_get_time_to_empty_min(void);
unsigned int battery_gauge_get_cell1_voltage_mv(void);
unsigned int battery_gauge_get_cell2_voltage_mv(void);
long battery_gauge_get_temperature_mdegc(void);
unsigned int battery_gauge_get_full_charge_capacity_mah(void);
