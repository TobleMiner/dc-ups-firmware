#pragma once

#include <stdint.h>

#include "i2c_bus.h"

typedef struct smbus {
	i2c_bus_t *i2c;
	uint8_t cmd_buf[I2C_LINK_RECOMMENDED_SIZE(5)];
	SemaphoreHandle_t lock;
	StaticSemaphore_t lock_buffer;
} smbus_t;

void smbus_init(smbus_t *bus, i2c_bus_t *i2c);

esp_err_t smbus_read(smbus_t* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len);
esp_err_t smbus_write(smbus_t* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len);
esp_err_t smbus_write_block(smbus_t* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len);
esp_err_t smbus_read_block(smbus_t* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len, size_t *data_len);

static inline esp_err_t smbus_read_byte(smbus_t* bus, uint8_t slave, uint8_t smcmd, void *data) {
	return smbus_read(bus, slave, smcmd, data, 1);
}

static inline esp_err_t smbus_write_byte(smbus_t* bus, uint8_t slave, uint8_t smcmd, void *data) {
	return smbus_write(bus, slave, smcmd, data, 1);
}

static inline esp_err_t smbus_read_word(smbus_t* bus, uint8_t slave, uint8_t smcmd, void *data) {
	return smbus_read(bus, slave, smcmd, data, 2);
}

static inline esp_err_t smbus_write_word(smbus_t* bus, uint8_t slave, uint8_t smcmd, void *data) {
	return smbus_write(bus, slave, smcmd, data, 2);
}
