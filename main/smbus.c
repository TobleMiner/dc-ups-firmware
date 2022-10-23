#include <stddef.h>
#include <stdlib.h>

#include <driver/i2c.h>
#include <esp_log.h>

#include "smbus.h"

static const char *TAG = "smbus";

void smbus_init(smbus_t *bus, i2c_bus_t *i2c) {
	bus->i2c = i2c;
	bus->lock = xSemaphoreCreateMutexStatic(&bus->lock_buffer);
}

esp_err_t smbus_write(smbus_t* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len) {
	xSemaphoreTake(bus->lock, portMAX_DELAY);
	esp_err_t err;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(bus->cmd_buf, sizeof(bus->cmd_buf));
	if(!cmd) {
		err = ESP_ERR_NO_MEM;
		goto fail;
	}
	if((err = i2c_master_start(cmd))) {
		goto fail_link;
	}
	if((err = i2c_master_write_byte(cmd, slave << 1, 1))) {
		goto fail_link;
	}
	if((err = i2c_master_write_byte(cmd, smcmd, 1))) {
		goto fail_link;
	}
	if((err = i2c_master_write(cmd, data, len, true))) {
		goto fail_link;
	}
	if((err = i2c_master_stop(cmd))) {
		goto fail_link;
	}
	err = i2c_bus_cmd_begin(bus->i2c, cmd, pdMS_TO_TICKS(100));
fail_link:
	i2c_cmd_link_delete_static(cmd);
fail:
	xSemaphoreGive(bus->lock);
	return err;
}

esp_err_t smbus_read(smbus_t* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len) {
	xSemaphoreTake(bus->lock, portMAX_DELAY);
	esp_err_t err;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(bus->cmd_buf, sizeof(bus->cmd_buf));
	if(!cmd) {
		err = ESP_ERR_NO_MEM;
		goto fail;
	}
	if((err = i2c_master_start(cmd))) {
		goto fail_link;
	}
	if((err = i2c_master_write_byte(cmd, slave << 1, 1))) {
		goto fail_link;
	}
	if((err = i2c_master_write_byte(cmd, smcmd, 1))) {
		goto fail_link;
	}
	if((err = i2c_master_start(cmd))) {
		goto fail_link;
	}
	if((err = i2c_master_write_byte(cmd, (slave << 1) | 1, 1))) {
		goto fail_link;
	}
	if((err = i2c_master_read(cmd, (uint8_t*)data, len, I2C_MASTER_LAST_NACK))) {
		goto fail_link;
	}
	if((err = i2c_master_stop(cmd))) {
		goto fail_link;
	}
	err = i2c_bus_cmd_begin(bus->i2c, cmd, pdMS_TO_TICKS(100));
fail_link:
	i2c_cmd_link_delete_static(cmd);
fail:
	xSemaphoreGive(bus->lock);
	return err;
}

esp_err_t smbus_write_block(smbus_t* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len) {
	xSemaphoreTake(bus->lock, portMAX_DELAY);
	esp_err_t err;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(bus->cmd_buf, sizeof(bus->cmd_buf));
	if(!cmd) {
		err = ESP_ERR_NO_MEM;
		goto fail;
	}
	if((err = i2c_master_start(cmd))) {
		goto fail_link;
	}
	if((err = i2c_master_write_byte(cmd, slave << 1, 1))) {
		goto fail_link;
	}
	if((err = i2c_master_write_byte(cmd, smcmd, 1))) {
		goto fail_link;
	}
	if((err = i2c_master_write_byte(cmd, len, 1))) {
		goto fail_link;
	}
	if((err = i2c_master_write(cmd, data, len, true))) {
		goto fail_link;
	}
	if((err = i2c_master_stop(cmd))) {
		goto fail_link;
	}
	err = i2c_bus_cmd_begin(bus->i2c, cmd, pdMS_TO_TICKS(1000));
fail_link:
	i2c_cmd_link_delete_static(cmd);
fail:
	xSemaphoreGive(bus->lock);
	return err;
}

esp_err_t smbus_read_block(smbus_t* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len, size_t *data_len) {
	xSemaphoreTake(bus->lock, portMAX_DELAY);
	esp_err_t err;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(bus->cmd_buf, sizeof(bus->cmd_buf));
	if(!cmd) {
		err = ESP_ERR_NO_MEM;
		goto fail;
	}
	if((err = i2c_master_start(cmd))) {
		goto fail_link;
	}
	if((err = i2c_master_write_byte(cmd, slave << 1, 1))) {
		goto fail_link;
	}
	if((err = i2c_master_write_byte(cmd, smcmd, 1))) {
		goto fail_link;
	}
	if((err = i2c_master_start(cmd))) {
		goto fail_link;
	}
	if((err = i2c_master_write_byte(cmd, (slave << 1) | 1, 1))) {
		goto fail_link;
	}
	uint8_t data_len_;
	if((err = i2c_master_read_byte(cmd, &data_len_, I2C_MASTER_ACK))) {
		goto fail_link;
	}
	if((err = i2c_master_read(cmd, (uint8_t*)data, len, I2C_MASTER_LAST_NACK))) {
		goto fail_link;
	}
	if((err = i2c_master_stop(cmd))) {
		goto fail_link;
	}
	err = i2c_bus_cmd_begin(bus->i2c, cmd, pdMS_TO_TICKS(1000));
	if (!err && data_len) {
		*data_len = data_len_;
	}
fail_link:
	i2c_cmd_link_delete_static(cmd);
fail:
	xSemaphoreGive(bus->lock);
	return err;
}
