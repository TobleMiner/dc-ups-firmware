#include <stdlib.h>
#include <string.h>

#include <esp_err.h>
#include <esp_log.h>
#include <rom/ets_sys.h>

#include "i2c_bus.h"
#include "util.h"

#define I2C_UNSTICK_BITS 32

static const char *TAG = "I2C_BUS";

static esp_err_t i2c_bus_init_(i2c_bus_t* bus) {
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = bus->gpio_sda,
		.scl_io_num = bus->gpio_scl,
		.master.clk_speed = bus->speed_hz,
	};

	esp_err_t err = i2c_param_config(bus->i2c_port, &i2c_config);
	if(err) {
		return err;
	}
	i2c_set_timeout(bus->i2c_port, 0xFFFFF);
	return i2c_driver_install(bus->i2c_port, I2C_MODE_MASTER, 0, 0, 0);
}

static esp_err_t i2c_bus_deinit_(i2c_bus_t* bus) {
	return i2c_driver_delete(bus->i2c_port);
}

esp_err_t i2c_bus_init(i2c_bus_t* bus, i2c_port_t i2c_port, unsigned int gpio_sda, unsigned int gpio_scl, uint32_t speed_hz) {
	memset(bus, 0, sizeof(*bus));
	bus->i2c_port = i2c_port;
	bus->gpio_sda = gpio_sda;
	bus->gpio_scl = gpio_scl;
	bus->speed_hz = speed_hz;

	esp_err_t err = i2c_bus_init_(bus);

	bus->lock = xSemaphoreCreateMutexStatic(&bus->lock_buffer);
	return err;
}

static void i2c_unstick_bus(i2c_bus_t* bus) {
	ESP_ERROR_CHECK(i2c_bus_deinit_(bus));

	ESP_ERROR_CHECK(gpio_reset_pin(bus->gpio_sda));
	ESP_ERROR_CHECK(gpio_set_direction(bus->gpio_sda, GPIO_MODE_INPUT));
	ESP_ERROR_CHECK(gpio_reset_pin(bus->gpio_scl));
	ESP_ERROR_CHECK(gpio_set_direction(bus->gpio_scl, GPIO_MODE_OUTPUT));
	for (int i = 0; i < I2C_UNSTICK_BITS; i++) {
		ESP_ERROR_CHECK(gpio_set_level(bus->gpio_scl, 0));
		ets_delay_us(DIV_ROUND_UP(1000000UL, bus->speed_hz));
		ESP_ERROR_CHECK(gpio_set_level(bus->gpio_scl, 1));
		ets_delay_us(DIV_ROUND_UP(1000000UL, bus->speed_hz));
	}

	ESP_ERROR_CHECK(i2c_bus_init_(bus));
}

esp_err_t i2c_bus_cmd_begin(i2c_bus_t* bus, i2c_cmd_handle_t handle, TickType_t timeout) {
	xSemaphoreTake(bus->lock, portMAX_DELAY);
	esp_err_t err = i2c_master_cmd_begin(bus->i2c_port, handle, timeout);

	if (err == ESP_ERR_TIMEOUT) {
		ESP_LOGE(TAG, "I2C bus timeout, trying to unstick bus");
		i2c_unstick_bus(bus);
	}
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
		esp_err_t nacked = i2c_bus_cmd_begin(bus, cmd, pdMS_TO_TICKS(100));
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
