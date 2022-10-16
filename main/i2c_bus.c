#include <stdlib.h>
#include <string.h>

#include <esp_err.h>
#include <esp_log.h>

#include "i2c_bus.h"

static const char *TAG = "I2C_BUS";

esp_err_t i2c_bus_init(i2c_bus_t* bus, i2c_port_t i2c_port, unsigned int gpio_sda, unsigned int gpio_scl, uint32_t speed) {
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = gpio_sda,
		.scl_io_num = gpio_scl,
		.master.clk_speed = speed,
	};

	esp_err_t err = i2c_param_config(i2c_port, &i2c_config);
	if(err) {
		return err;
	}
	err = i2c_driver_install(i2c_port, I2C_MODE_MASTER, 0, 0, 0);
	if(err) {
		return err;
	}

	memset(bus, 0, sizeof(*bus));
	bus->i2c_port = i2c_port;
	bus->lock = xSemaphoreCreateMutexStatic(&bus->lock_buffer);
	return err;
}

esp_err_t i2c_bus_cmd_begin(i2c_bus_t* bus, i2c_cmd_handle_t handle, TickType_t timeout) {
	xSemaphoreTake(bus->lock, portMAX_DELAY);
	esp_err_t err = i2c_master_cmd_begin(bus->i2c_port, handle, timeout);
	xSemaphoreGive(bus->lock);
	return err;
}

esp_err_t i2c_bus_scan(i2c_bus_t* bus, i2c_address_set_t addr) {
	esp_err_t err = ESP_OK;
	size_t link_buf_size = I2C_LINK_RECOMMENDED_SIZE(1);
	void *link_buf = malloc(link_buf_size);
	for (uint8_t i = 0; i < 128; i++) {
		I2C_ADDRESS_SET_CLEAR(addr, i);
		i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(link_buf, link_buf_size);
		if(!cmd) {
			err = ESP_ERR_NO_MEM;
			continue;
		}
		if((err = i2c_master_start(cmd))) {
			goto fail_link;
		}
		if((err = i2c_master_write_byte(cmd, (i << 1), true))) {
			goto fail_link;
		}
		if((err = i2c_master_stop(cmd))) {
			goto fail_link;
		}
		esp_err_t nacked = i2c_bus_cmd_begin(bus, cmd, 100 / portTICK_RATE_MS);
		if(!nacked) {
			I2C_ADDRESS_SET_SET(addr, i);
		}
		i2c_cmd_link_delete_static(cmd);
		continue;
fail_link:
		i2c_cmd_link_delete_static(cmd);
		break;
	}
	free(link_buf);
	return err;
}

void i2c_detect(i2c_bus_t* bus) {
	ESP_LOGI(TAG, "Scanning i2c bus %d for devices", bus->i2c_port);
	I2C_ADDRESS_SET(devices);
	esp_err_t err = i2c_bus_scan(bus, devices);
	if (err) {
		ESP_LOGE(TAG, "Failed to scan bus %d: %d", bus->i2c_port, err);
	} else {
		ESP_LOGI(TAG, "=== Detected devices ===");
		for (uint8_t i = 0; i < 128; i++) {
			if (I2C_ADDRESS_SET_CONTAINS(devices, i)) {
				ESP_LOGI(TAG, "  0x%02x", i);
			}
		}
		ESP_LOGI(TAG, "========================");
	}
}
