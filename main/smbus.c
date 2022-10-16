#include <stddef.h>
#include <stdlib.h>

#include <driver/i2c.h>

#include "smbus.h"

esp_err_t smbus_write(struct i2c_bus* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len) {
	esp_err_t err;
	size_t cmd_buf_len = I2C_LINK_RECOMMENDED_SIZE(3);
	void *cmd_buf = malloc(cmd_buf_len);
	i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(cmd_buf, cmd_buf_len);
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
	err = i2c_bus_cmd_begin(bus, cmd, pdMS_TO_TICKS(100));
fail_link:
	i2c_cmd_link_delete_static(cmd);
fail:
	return err;
}

esp_err_t smbus_read(struct i2c_bus* bus, uint8_t slave, uint8_t smcmd, void *data, size_t len) {
	esp_err_t err;
	size_t cmd_buf_len = I2C_LINK_RECOMMENDED_SIZE(4);
	void *cmd_buf = malloc(cmd_buf_len);
	i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(cmd_buf, cmd_buf_len);
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
	err = i2c_bus_cmd_begin(bus, cmd, pdMS_TO_TICKS(100));
fail_link:
	i2c_cmd_link_delete_static(cmd);
fail:
	return err;
}
