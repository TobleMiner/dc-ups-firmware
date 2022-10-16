#include "bq24715_charger.h"
#include "smbus.h"

#define SMBUS_ADDRESS	0x09
#define DEVICE_ID	0x10
#define MANUFACTURER_ID	0x40

#define CMD_CHARGE_CURRENT	0x14
#define CMD_MAX_CHARGE_VOLTAGE	0x15
#define CMD_MIN_SYSTEM_VOLTAGE	0x3e
#define CMD_INPUT_CURRENT	0x3f
#define CMD_MANUFACTURER_ID	0xfe
#define CMD_DEVICE_ID		0xff

esp_err_t bq24715_init(bq24715_t *charger, i2c_bus_t *smbus) {
	uint8_t word[2];
	esp_err_t err = smbus_read_word(smbus, SMBUS_ADDRESS, CMD_MANUFACTURER_ID, word);
	if (err) {
		return err;
	}
	if (word[0] != MANUFACTURER_ID) {
		return ESP_ERR_INVALID_RESPONSE;
	}
	err = smbus_read_word(smbus, SMBUS_ADDRESS, CMD_DEVICE_ID, word);
	if (err) {
		return err;
	}
	if (word[0] != DEVICE_ID) {
		return ESP_ERR_INVALID_RESPONSE;
	}

	charger->bus = smbus;
	return ESP_OK;
}

esp_err_t bq24715_set_charge_current(bq24715_t *charger, unsigned int current_ma) {
	if (current_ma < 128 || current_ma > 8192) {
		return ESP_ERR_INVALID_ARG;
	}
	current_ma &= ~((uint16_t)0xe03f);
	uint8_t word[2] = { current_ma & 0xff, current_ma >> 8 };
	return smbus_write_word(charger->bus, SMBUS_ADDRESS, CMD_CHARGE_CURRENT, word);
}

esp_err_t bq24715_set_max_charge_voltage(bq24715_t *charger, unsigned int voltage_mv) {
	if (voltage_mv < 4096 || voltage_mv > 14500) {
		return ESP_ERR_INVALID_ARG;
	}
	voltage_mv &= ~((uint16_t)0x0f);
	uint8_t word[2] = { voltage_mv & 0xff, voltage_mv >> 8 };
	return smbus_write_word(charger->bus, SMBUS_ADDRESS, CMD_MAX_CHARGE_VOLTAGE, word);
}

esp_err_t bq24715_set_min_system_voltage(bq24715_t *charger, unsigned int voltage_mv) {
	if (voltage_mv < 4096 || voltage_mv > 14500) {
		return ESP_ERR_INVALID_ARG;
	}
	voltage_mv &= ~((uint16_t)0xff);
	uint8_t word[2] = { voltage_mv & 0xff, voltage_mv >> 8 };
	return smbus_write_word(charger->bus, SMBUS_ADDRESS, CMD_MIN_SYSTEM_VOLTAGE, word);
}

esp_err_t bq24715_set_input_current(bq24715_t *charger, unsigned int current_ma) {
	if (current_ma < 128 || current_ma > 8064) {
		return ESP_ERR_INVALID_ARG;
	}
	current_ma &= ~((uint16_t)0x3f);
	uint8_t word[2] = { current_ma & 0xff, current_ma >> 8 };
	return smbus_write_word(charger->bus, SMBUS_ADDRESS, CMD_INPUT_CURRENT, word);
}
