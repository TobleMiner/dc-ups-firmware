#pragma once

#include <stdint.h>

#include "i2c_bus.h"

esp_err_t smbus_read(struct i2c_bus* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len);
esp_err_t smbus_write(struct i2c_bus* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len);
esp_err_t smbus_write_block(struct i2c_bus* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len);
esp_err_t smbus_read_block(struct i2c_bus* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len, size_t *data_len);

static inline esp_err_t smbus_read_byte(struct i2c_bus* bus, uint8_t slave, uint8_t smcmd, void *data) {
	return smbus_read(bus, slave, smcmd, data, 1);
}

static inline esp_err_t smbus_write_byte(struct i2c_bus* bus, uint8_t slave, uint8_t smcmd, void *data) {
	return smbus_write(bus, slave, smcmd, data, 1);
}

static inline esp_err_t smbus_read_word(struct i2c_bus* bus, uint8_t slave, uint8_t smcmd, void *data) {
	return smbus_read(bus, slave, smcmd, data, 2);
}

static inline esp_err_t smbus_write_word(struct i2c_bus* bus, uint8_t slave, uint8_t smcmd, void *data) {
	return smbus_write(bus, slave, smcmd, data, 2);
}
