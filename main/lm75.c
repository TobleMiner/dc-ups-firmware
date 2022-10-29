#include "lm75.h"

#include "util.h"

static unsigned int get_num_channels(sensor_t *sensor, sensor_measurement_type_t type) {
	switch (type) {
	case SENSOR_TYPE_TEMPERATURE:
		return 1;
	default:
		return 0;
	}
}

static esp_err_t measure(sensor_t *sensor, sensor_measurement_type_t type, unsigned int channel, long *res) {
	lm75_t *lm75 = container_of(sensor, lm75_t, sensor);
	switch (type) {
	case SENSOR_TYPE_TEMPERATURE: {
		int32_t temperature_mdegc;
		esp_err_t err = lm75_read_temperature_mdegc(lm75, &temperature_mdegc);
		if (err) {
			return err;
		}
		*res = temperature_mdegc;
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

void lm75_init(lm75_t *lm75, i2c_bus_t *bus, unsigned int address, const char *name) {
	lm75->bus = bus;
	lm75->address = address;
	sensor_init(&lm75->sensor, &sensor_def, name);
	sensor_add(&lm75->sensor);
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
