#include <driver/i2c.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include "bq24715_charger.h"
#include "bq40z50_gauge.h"
#include "gpio_hc595.h"
#include "i2c_bus.h"
#include "lm75.h"
#include "util.h"

#define GPIO_HC595_DC_OUT3_OFF	1
#define GPIO_HC595_DC_OUT_TEST	2
#define GPIO_HC595_DC_OUT_OFF	3
#define GPIO_HC595_USB_OUT_OFF	4
#define GPIO_HC595_DC_OUT2_OFF	5
#define GPIO_HC595_DC_OUT1_OFF	6

#define SPI_HC595		SPI2_HOST
#define GPIO_HC595_DATA		16
#define GPIO_HC595_CLK		14
#define GPIO_HC595_LATCH	33

#define I2C_SMBUS		I2C_NUM_1
#define GPIO_SMBUS_DATA		5
#define GPIO_SMBUS_CLK		4

#define I2C_I2C			I2C_NUM_0
#define GPIO_I2C_DATA		15
#define GPIO_I2C_CLK		13

static const char *TAG = "main";

static const spi_bus_config_t hc595_spi_bus_cfg = {
	.mosi_io_num = GPIO_HC595_DATA,
	.miso_io_num = -1,
	.sclk_io_num = GPIO_HC595_CLK,
	.max_transfer_sz = 16,
	.flags = SPICOMMON_BUSFLAG_MASTER
};

static gpio_hc595_t hc595;

static i2c_bus_t i2c_bus;
static i2c_bus_t smbus_bus;

static lm75_t lm75[3];
static unsigned int lm75_address[3] = { 0x48, 0x49, 0x4a };

static bq24715_t bq24715;
static bq40z50_t bq40z50;

void app_main() {
	ESP_ERROR_CHECK(spi_bus_initialize(SPI_HC595, &hc595_spi_bus_cfg, SPI_DMA_DISABLED));
	ESP_ERROR_CHECK(gpio_hc595_init(&hc595, SPI_HC595, GPIO_HC595_LATCH));

	ESP_ERROR_CHECK(i2c_bus_init(&smbus_bus, I2C_SMBUS, GPIO_SMBUS_DATA, GPIO_SMBUS_CLK, KHZ(100)));
	i2c_detect(&smbus_bus);

	ESP_ERROR_CHECK(i2c_bus_init(&i2c_bus, I2C_I2C, GPIO_I2C_DATA, GPIO_I2C_CLK, KHZ(100)));
	i2c_detect(&i2c_bus);

	for (int i = 0; i < ARRAY_SIZE(lm75); i++) {
		lm75_init(&lm75[i], &i2c_bus, lm75_address[i]);
		int32_t temp_mdegc = lm75_read_temperature_mdegc(&lm75[i]);
		if (temp_mdegc < 0) {
			ESP_LOGI(TAG, "Temperature readout failed on sensor %d", i);
		} else {
			ESP_LOGI(TAG, "Sensor %d: %.2f°C", i, (float)temp_mdegc / 1000.0f);
		}
	}

	ESP_ERROR_CHECK(bq24715_init(&bq24715, &smbus_bus));
	ESP_ERROR_CHECK(bq24715_set_max_charge_voltage(&bq24715, 8400));
	ESP_ERROR_CHECK(bq24715_set_charge_current(&bq24715, 256));

	ESP_ERROR_CHECK(bq40z50_init(&bq40z50, &smbus_bus, -1));

	unsigned int toggle_gpios[] = { GPIO_HC595_USB_OUT_OFF, GPIO_HC595_DC_OUT1_OFF, GPIO_HC595_DC_OUT2_OFF, GPIO_HC595_DC_OUT3_OFF, GPIO_HC595_DC_OUT_OFF };
	unsigned int gpio_idx = 0;
	while (1) {
		ESP_LOGI(TAG, "Toggling GPIO %u", gpio_idx);
		unsigned int gpio = toggle_gpios[gpio_idx++];
		gpio_idx %= ARRAY_SIZE(toggle_gpios);
		ESP_ERROR_CHECK(gpio_hc595_set_level(&hc595, gpio, 1));
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_ERROR_CHECK(gpio_hc595_set_level(&hc595, gpio, 0));
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
