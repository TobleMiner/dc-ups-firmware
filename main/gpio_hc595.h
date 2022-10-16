#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <driver/spi_master.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

typedef struct gpio_hc595 {
	uint8_t io_state;
	unsigned int latch_gpio;
	spi_device_handle_t spidev;
	SemaphoreHandle_t lock;
	StaticSemaphore_t lock_buffer;
} gpio_hc595_t;

esp_err_t gpio_hc595_init(gpio_hc595_t *hc595, spi_host_device_t hostdev, unsigned int latch_gpio);
esp_err_t gpio_hc595_set_level(gpio_hc595_t *hc595, unsigned int gpio, bool level);
