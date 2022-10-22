#include <driver/gpio.h>
#include <driver/i2c.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include "bq24715_charger.h"
#include "bq40z50_gauge.h"
#include "font_3x5.h"
#include "gpio_hc595.h"
#include "i2c_bus.h"
#include "ina219.h"
#include "lm75.h"
#include "ssd1306_oled.h"
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

#define GPIO_OLED_RESET		23

#define GPIO_BUTTON		39

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

static ina219_t ina[4];
static unsigned int ina_address[4] = { 0x40, 0x41, 0x42, 0x43 };

static ssd1306_oled_t oled;
static fb_t fb;

static volatile bool do_shutdown = false;

static void button_pressed(void *_) {
	do_shutdown = true;
}

void app_main() {
	ESP_ERROR_CHECK(gpio_install_isr_service(0));

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

	ESP_ERROR_CHECK(ssd1306_oled_init(&oled, &i2c_bus, 0x3c, GPIO_OLED_RESET));
	fb_init(&fb);
	font_3x5_render_string("Hello World", &fb, 0, 0);
	font_3x5_render_string("Hello World", &fb, 0, 6);
	font_3x5_render_string("Hello World", &fb, 0, 12);
	ESP_ERROR_CHECK(ssd1306_oled_render_fb(&oled, &fb));

	ESP_ERROR_CHECK(bq24715_init(&bq24715, &smbus_bus));
	ESP_ERROR_CHECK(bq24715_set_max_charge_voltage(&bq24715, 8400));
	ESP_ERROR_CHECK(bq24715_set_charge_current(&bq24715, 256));

	ESP_ERROR_CHECK(bq40z50_init(&bq40z50, &smbus_bus, -1));
	gpio_set_direction(GPIO_BUTTON, GPIO_MODE_INPUT);
	gpio_set_intr_type(GPIO_BUTTON, GPIO_INTR_NEGEDGE);
	ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_BUTTON, button_pressed, NULL));

	for (int i = 0; i < ARRAY_SIZE(ina); i++) {
		ESP_ERROR_CHECK(ina219_init(&ina[i], &smbus_bus, ina_address[i], 10));
		ESP_ERROR_CHECK(ina219_set_shunt_voltage_range(&ina[i], INA219_PGA_CURRENT_GAIN_80MV));
		unsigned int bus_voltage_mv;
		long current_ua, shunt_voltage_uv, power_uw;
		ESP_ERROR_CHECK(ina219_read_shunt_voltage_uv(&ina[i], &shunt_voltage_uv));
		ESP_ERROR_CHECK(ina219_read_bus_voltage_mv(&ina[i], &bus_voltage_mv));
		ESP_ERROR_CHECK(ina219_read_current_ua(&ina[i], &current_ua));
		ESP_ERROR_CHECK(ina219_read_power_uw(&ina[i], &power_uw));
		ESP_LOGI(TAG, "INA %d: %umV @ %ldmA (Ushunt: %lduV, %ldmW)", i, bus_voltage_mv, current_ua / 1000L, shunt_voltage_uv, power_uw / 1000L);
	}

	unsigned int toggle_gpios[] = { GPIO_HC595_USB_OUT_OFF, GPIO_HC595_DC_OUT1_OFF, GPIO_HC595_DC_OUT2_OFF, GPIO_HC595_DC_OUT3_OFF, GPIO_HC595_DC_OUT_OFF };
	unsigned int gpio_idx = 0;
	unsigned int pixel = 0;
	unsigned int last_pixel = pixel;

	while (1) {
		ESP_LOGI(TAG, "Toggling GPIO %u", gpio_idx);
		unsigned int gpio = toggle_gpios[gpio_idx++];
		gpio_idx %= ARRAY_SIZE(toggle_gpios);
//		ESP_ERROR_CHECK(gpio_hc595_set_level(&hc595, gpio, 1));
//		vTaskDelay(pdMS_TO_TICKS(1000));
//		ESP_ERROR_CHECK(gpio_hc595_set_level(&hc595, gpio, 0));
		vTaskDelay(pdMS_TO_TICKS(1000));

		unsigned int cell_voltage1, cell_voltage2;
		int current_ma;
		ESP_ERROR_CHECK(bq40z50_get_cell_voltage_mv(&bq40z50, BQ40Z50_CELL_1, &cell_voltage1));
		ESP_ERROR_CHECK(bq40z50_get_cell_voltage_mv(&bq40z50, BQ40Z50_CELL_2, &cell_voltage2));
		ESP_LOGI(TAG, "Cell voltages: %umV, %umV", cell_voltage1, cell_voltage2);
		ESP_ERROR_CHECK(bq40z50_get_current_ma(&bq40z50, BQ40Z50_CELL_2, &current_ma));
		if (current_ma > 0) {
			ESP_LOGI(TAG, "Charging at %dmA", current_ma);
		} else {
			ESP_LOGI(TAG, "Discharging at %dmA", -current_ma);
		}

/*
		for (int i = 0; i < 10; i++) {
			ESP_ERROR_CHECK(ssd1306_oled_set_pixel(&oled, last_pixel % 64, last_pixel / 64, false));
			last_pixel = pixel;
			ESP_ERROR_CHECK(ssd1306_oled_set_pixel(&oled, pixel % 64, pixel / 64, true));
			pixel++;
			pixel %= 48 * 64;
			vTaskDelay(pdMS_TO_TICKS(100));
		}
*/
		if (do_shutdown) {
			ESP_LOGI(TAG, "Button pressed! Shutting down battery pack...");
			bq40z50_shutdown(&bq40z50);
			do_shutdown = false;
		}
	}
}
