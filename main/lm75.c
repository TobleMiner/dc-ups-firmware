#include "lm75.h"

void lm75_init(lm75_t *lm75, i2c_bus_t *bus, unsigned int address) {
	lm75->bus = bus;
	lm75->address = address;
}

int32_t lm75_read_temperature_mdegc(lm75_t *lm75) {
	i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(lm75->xfers, sizeof(lm75->xfers));
	uint8_t temp_data[2];
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (lm75->address << 1), true);
	i2c_master_write_byte(cmd, 0, true);
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (lm75->address << 1) | 1, true);
	i2c_master_read(cmd, temp_data, sizeof(temp_data), I2C_MASTER_LAST_NACK);
	i2c_master_stop(cmd);
	esp_err_t err = i2c_bus_cmd_begin(lm75->bus, cmd, pdMS_TO_TICKS(10));
	i2c_cmd_link_delete_static(cmd);
	if (err) {
		return -1;
	}
	uint16_t temp_0_125C =
		(((int16_t)temp_data[0]) << 8) |
                temp_data[1];
	temp_0_125C &= ~((int16_t)0x1f);
        temp_0_125C /= 32;
	return (int32_t)temp_0_125C * 125;
}
