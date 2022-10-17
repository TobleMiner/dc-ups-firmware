#include "bq40z50_gauge.h"

#include <esp_log.h>

#include "smbus.h"

#define DEFAULT_SMBUS_ADDRESS		0x0b

#define DEVICE_TYPE			0x4500

#define CMD_AT_RATE_TIME_TO_EMPTY	0x06
#define CMD_TEMPERATURE			0x08
#define CMD_VOLTAGE			0x09
#define CMD_CURRENT			0x0a
#define CMD_AVERAGE_CURRENT		0x0b
#define CMD_STATE_OF_CHARGE		0x0d
#define CMD_REMAINING_ENERGY		0x0f
#define CMD_RUN_TIME_TO_EMPTY		0x11
#define CMD_AVERAGE_TIME_TO_EMPTY	0x12
#define CMD_CELL_VOLTAGE2		0x3e
#define CMD_CELL_VOLTAGE1		0x3f
#define CMD_STATE_OF_HEALTH		0x4f

#define CMD_MANUFACTURER_ACCESS		0x44
#define CMD_MAC_DEVICE_TYPE		0x01

static const char *TAG = "BQ40Z50 GAUGE";

static esp_err_t read_uword(bq40z50_t *gauge, uint8_t cmd, unsigned int *res) {
	uint8_t word[2];
	esp_err_t err = smbus_read_word(gauge->bus, gauge->address, cmd, word);
	if (!err) {
		*res = (unsigned int)word[0] | (unsigned int)word[1] << 8;
	}
	return err;
}

static esp_err_t read_sword(bq40z50_t *gauge, uint8_t cmd, int *res) {
	uint8_t word[2];
	esp_err_t err = smbus_read_word(gauge->bus, gauge->address, cmd, word);
	if (!err) {
		*res = (int16_t)((int16_t)word[0] | (int16_t)word[1] << 8);
	}
	return err;
}

static esp_err_t write_word(bq40z50_t *gauge, uint8_t cmd, uint16_t val) {
	uint8_t word[2] = { val & 0xff, val >> 8 };
	return smbus_write_word(gauge->bus, gauge->address, cmd, word);
}

static esp_err_t read_mac_word(bq40z50_t *gauge, uint16_t cmd, uint16_t *res) {
	uint8_t response[4];
	uint8_t word[2] = { cmd & 0xff, cmd >> 8 };
	esp_err_t err = smbus_write_block(gauge->bus, gauge->address, CMD_MANUFACTURER_ACCESS, word, sizeof(word));
	if (err) {
		ESP_LOGE(TAG, "Failed to write MAC command: 0x%x(%d)", err, err);
		return err;
	}
	err = smbus_read_block(gauge->bus, gauge->address, CMD_MANUFACTURER_ACCESS, response, sizeof(response), NULL);
	if (err) {
		ESP_LOGE(TAG, "Failed to read MAC command response: 0x%x(%d)", err, err);
		return err;
	}
	uint16_t cmd_readback = (uint16_t)response[0] | (uint16_t)response[1] << 8;
	if (cmd_readback != cmd) {
		ESP_LOGE(TAG, "Invalid MAC response! expected 0x%04x, but got 0x%04x. Race condition?", cmd, cmd_readback);
		return ESP_ERR_INVALID_RESPONSE;
	}
	*res = (uint16_t)response[2] | (uint16_t)response[3] << 8;
	return ESP_OK;
}

esp_err_t bq40z50_init(bq40z50_t *gauge, i2c_bus_t *bus, int address) {
	if (address == -1) {
		address = DEFAULT_SMBUS_ADDRESS;
	}
	gauge->bus = bus;
	gauge->address = address;
	uint16_t device_type;
	esp_err_t err = read_mac_word(gauge, CMD_MAC_DEVICE_TYPE, &device_type);
	if (err) {
		ESP_LOGE(TAG, "Failed to check device type: %d", err);
		return err;
	}

	if (device_type != DEVICE_TYPE) {
		ESP_LOGE(TAG, "Incompatible device type detected (is 0x%04x, should be 0x%04x)", device_type, DEVICE_TYPE);
		return ESP_ERR_INVALID_RESPONSE;
	}

	return ESP_OK;
}

esp_err_t bq40z50_get_battery_voltage_mv(bq40z50_t *gauge, unsigned int *res) {
	return read_uword(gauge, CMD_VOLTAGE, res);
}

esp_err_t bq40z50_get_cell_voltage_mv(bq40z50_t *gauge, bq40z50_cell_t cell, unsigned int *res) {
	switch (cell) {
	case BQ40Z50_CELL_1:
		return read_uword(gauge, CMD_CELL_VOLTAGE1, res);
	case BQ40Z50_CELL_2:
		return read_uword(gauge, CMD_CELL_VOLTAGE2, res);
	default:
		return ESP_ERR_INVALID_ARG;
	}
}

esp_err_t bq40z50_get_state_of_charge_percent(bq40z50_t *gauge, bq40z50_cell_t cell, unsigned int *res) {
	return read_uword(gauge, CMD_STATE_OF_CHARGE, res);
}

esp_err_t bq40z50_get_current_ma(bq40z50_t *gauge, bq40z50_cell_t cell, int *res) {
	return read_sword(gauge, CMD_CURRENT, res);
}
