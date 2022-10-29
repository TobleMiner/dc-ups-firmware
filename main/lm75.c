#include "lm75.h"

#include "util.h"

static esp_err_t read_temperature(temperature_sensor_t *sensor, int32_t *res) {
	lm75_t *lm75 = container_of(sensor, lm75_t, sensor);
	return lm75_read_temperature_mdegc(lm75, res);
}

const temperature_sensor_ops_t temp_ops = {
	.read_temperature_mdegc = read_temperature,
};

void lm75_init(lm75_t *lm75, i2c_bus_t *bus, unsigned int address, const char *name) {
	lm75->bus = bus;
	lm75->address = address;
	temperature_sensor_init(&lm75->sensor, name, &temp_ops);
}

esp_err_t lm75_read_temperature_mdegc(lm75_t *lm75, int32_t *res) {
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
		return err;
	}
	uint16_t temp_0_125C =
		(((int16_t)temp_data[0]) << 8) |
                temp_data[1];
	temp_0_125C &= ~((int16_t)0x1f);
        temp_0_125C /= 32;
	*res = (int32_t)temp_0_125C * 125;
        return ESP_OK;
}
