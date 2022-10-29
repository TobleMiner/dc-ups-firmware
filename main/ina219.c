#include "ina219.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define CMD_CONFIGURATION	0x00
#define CMD_SHUNT_VOLTAGE	0x01
#define CMD_BUS_VOLTAGE		0x02
#define CMD_POWER		0x03
#define CMD_CURRENT		0x04
#define CMD_CALIBRATION		0x05

#define CONFIGURATION_RESET	(1 << 15)

static const char *TAG = "INA219";

static esp_err_t write_word(ina219_t *ina, unsigned int cmd, uint16_t val) {
	uint8_t word[2] = { val >> 8, val & 0xff };
	return smbus_write_word(ina->bus, ina->address, cmd, word);
}

static esp_err_t read_uword(ina219_t *ina, unsigned int cmd, unsigned int *res) {
	uint8_t word[2];
	esp_err_t err = smbus_read_word(ina->bus, ina->address, cmd, word);
	if (!err) {
		*res = (uint16_t)((uint16_t)word[1] | ((uint16_t)word[0] << 8));
	}
	return err;
}

static esp_err_t read_sword(ina219_t *ina, unsigned int cmd, int *res) {
	uint8_t word[2];
	esp_err_t err = smbus_read_word(ina->bus, ina->address, cmd, word);
	if (!err) {
		*res = (int16_t)((int16_t)word[1] | ((int16_t)word[0] << 8));
	}
	return err;
}

static esp_err_t update_bits(ina219_t *ina, unsigned int cmd, unsigned int shift, unsigned int mask, unsigned int val) {
	unsigned int old_value;
	esp_err_t err = read_uword(ina, cmd, &old_value);
	if (err) {
		return err;
	}
	uint16_t new_value = old_value & ~(uint16_t)(mask << shift);
	new_value |= val << shift;
	return write_word(ina, cmd, new_value);
}

static unsigned int get_num_channels(sensor_t *sensor, sensor_measurement_type_t type) {
	switch (type) {
	case SENSOR_TYPE_VOLTAGE:
		return 1;
	case SENSOR_TYPE_CURRENT:
		return 1;
	case SENSOR_TYPE_POWER:
		return 1;
	default:
		return 0;
	}
}

static esp_err_t measure(sensor_t *sensor, sensor_measurement_type_t type, unsigned int channel, long *res) {
	ina219_t *ina = container_of(sensor, ina219_t, sensor);
	switch (type) {
	case SENSOR_TYPE_VOLTAGE: {
		unsigned int voltage_mv;
		esp_err_t err = ina219_read_bus_voltage_mv(ina, &voltage_mv);
		if (err) {
			return err;
		}
		*res = voltage_mv;
		break;
	}
	case SENSOR_TYPE_CURRENT: {
		long current_ua;
		esp_err_t err = ina219_read_current_ua(ina, &current_ua);
		if (err) {
			return err;
		}
		*res = current_ua / 1000;
		break;
	}
	case SENSOR_TYPE_POWER:{
		long power_uw;
		esp_err_t err = ina219_read_power_uw(ina, &power_uw);
		if (err) {
			return err;
		}
		*res = power_uw / 1000;
		break;
	}
	default:
		return ESP_ERR_INVALID_ARG;
	}

	return ESP_OK;
}

static const sensor_def_t sensor_def = {
	.get_num_channels = get_num_channels,
	.get_channel_name = NULL,
	.measure = measure,
};

esp_err_t ina219_init(ina219_t *ina, smbus_t *bus, unsigned int address, unsigned int shunt_resistance_mohms, const char *name) {
	ina->bus = bus;
	ina->address = address;
	ina->shunt_resistance_mohms = shunt_resistance_mohms;

	esp_err_t err = ina219_reset(ina);
	if (err) {
		ESP_LOGE(TAG, "Failed to reset INA219");
		return err;
	}
	sensor_init(&ina->sensor, &sensor_def, name);
	sensor_add(&ina->sensor);

	return err;
}

esp_err_t ina219_reset(ina219_t *ina) {
	esp_err_t err = write_word(ina, CMD_CONFIGURATION, CONFIGURATION_RESET);
	if (!err) {
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	return err;
}

esp_err_t ina219_set_voltage_range(ina219_t *ina, ina219_bus_voltage_range_t range) {
	switch (range) {
	case INA219_BUS_VOLTAGE_RANGE_16V:
		return update_bits(ina, CMD_CONFIGURATION, 13, 1, 0);
	case INA219_BUS_VOLTAGE_RANGE_32V:
		return update_bits(ina, CMD_CONFIGURATION, 13, 1, 1);
	}

	return ESP_ERR_INVALID_ARG;
}

esp_err_t ina219_set_shunt_voltage_range(ina219_t *ina, ina219_pga_current_gain_t gain) {
	return update_bits(ina, CMD_CONFIGURATION, 11, 3, gain);
}

esp_err_t ina219_set_shunt_voltage_resolution(ina219_t *ina, ina219_adc_resolution_t resolution) {
	return update_bits(ina, CMD_CONFIGURATION, 3, 15, resolution);
}

esp_err_t ina219_set_bus_voltage_adc_resolution(ina219_t *ina, ina219_adc_resolution_t resolution) {
	return update_bits(ina, CMD_CONFIGURATION, 7, 15, resolution);
}

esp_err_t ina219_read_shunt_voltage_uv(ina219_t *ina, long *shunt_voltage_uv) {
	int shunt_voltage_raw;
	esp_err_t err = read_sword(ina, CMD_SHUNT_VOLTAGE, &shunt_voltage_raw);
	if (!err) {
		*shunt_voltage_uv = (long)shunt_voltage_raw * 10L;
	}
	return err;
}

esp_err_t ina219_read_bus_voltage_mv(ina219_t *ina, unsigned int *bus_voltage_mv) {
	unsigned int bus_voltage_reg;
	esp_err_t err = read_uword(ina, CMD_BUS_VOLTAGE, &bus_voltage_reg);
	if (!err) {
		*bus_voltage_mv = (bus_voltage_reg & ~0x07U) >> 1;
	}
	return err;
}

esp_err_t ina219_read_current_ua(ina219_t *ina, long *current_ua) {
	long shunt_voltage_uv;
	esp_err_t err = ina219_read_shunt_voltage_uv(ina, &shunt_voltage_uv);
	if (!err) {
		*current_ua = (int32_t)shunt_voltage_uv * (int32_t)1000 /
				(int32_t)ina->shunt_resistance_mohms;
	}
	return err;
}

esp_err_t ina219_read_power_uw(ina219_t *ina, long *power_uw) {
	long current_ua;
	unsigned int voltage_mv;
	esp_err_t err = ina219_read_bus_voltage_mv(ina, &voltage_mv);
	if (err) {
		ESP_LOGE(TAG, "Failed to read bus voltage");
		return err;
	}
	err = ina219_read_current_ua(ina, &current_ua);
	if (err) {
		ESP_LOGE(TAG, "Failed to read current");
		return err;
	}
	*power_uw = (int64_t)current_ua * (int64_t)voltage_mv / (int64_t)1000;
	return ESP_OK;
}

