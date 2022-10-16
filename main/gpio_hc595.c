#include "gpio_hc595.h"

#include "delay.h"

#include <driver/gpio.h>
#include <esp_log.h>

static const char *TAG = "GPIO_HC595";

static const spi_device_interface_config_t hc595_spicfg = {
	.clock_speed_hz = 1 * 1000 * 1000,
	.spics_io_num = -1,
	.queue_size = 1,
};

esp_err_t gpio_hc595_init(gpio_hc595_t *hc595, spi_host_device_t hostdev, unsigned int latch_gpio) {
	esp_err_t err = gpio_set_direction(latch_gpio, GPIO_MODE_OUTPUT);
	if (err) {
		return err;
	}

	err = gpio_set_level(latch_gpio, 1);
	if (err) {
		return err;
	}

	hc595->io_state = 0;
	hc595->latch_gpio = latch_gpio;
	hc595->lock = xSemaphoreCreateMutexStatic(&hc595->lock_buffer);
	return spi_bus_add_device(hostdev, &hc595_spicfg, &hc595->spidev);
}

esp_err_t gpio_hc595_set_level(gpio_hc595_t *hc595, unsigned int gpio, bool level) {
	if (gpio > 7) {
		ESP_LOGE(TAG, "gpio can not be higher than 7");
		return ESP_ERR_INVALID_ARG;
	}

	xSemaphoreTake(hc595->lock, portMAX_DELAY);
	uint8_t io_state = hc595->io_state;
	/* SPI is MSB first, no need to swap bit order */
	if (level) {
		io_state |= (1U << gpio);
	} else {
		io_state &= ~((uint8_t)1U << gpio);
	}
	spi_transaction_t xfer = {
		.flags = SPI_TRANS_USE_TXDATA,
		.length = 8,
		.tx_data = { io_state },
	};
	esp_err_t err = spi_device_transmit(hc595->spidev, &xfer);	
	if (!err) {
		gpio_set_level(hc595->latch_gpio, 0);
		delay_us(10);
		gpio_set_level(hc595->latch_gpio, 1);
	}
	xSemaphoreGive(hc595->lock);
	return err;
}
