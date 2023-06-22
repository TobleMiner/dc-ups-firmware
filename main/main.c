#include <driver/gpio.h>
#include <driver/i2c.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_spiffs.h>
#include <freertos/FreeRTOS.h>

#include "api.h"
#include "battery_protection.h"
#include "bq40z50_gauge.h"
#include "buttons.h"
#include "display.h"
#include "ethernet.h"
#include "event_bus.h"
#include "font_3x5.h"
#include "gpio_hc595.h"
#include "httpd.h"
#include "i2c_bus.h"
#include "power_path.h"
#include "prometheus_exporter.h"
#include "prometheus_metrics.h"
#include "prometheus_metrics_battery.h"
#include "scheduler.h"
#include "sensor.h"
#include "settings.h"
#include "ssd1306_oled.h"
#include "util.h"
#include "vendor.h"
#include "website.h"
#include "wifi.h"

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

#define GPIO_BUTTON		39

#define GPIO_PHY_RESET		32
#define GPIO_PHY_MDC		2
#define GPIO_PHY_MDIO		18

static const char *TAG = "main";

static const esp_vfs_spiffs_conf_t spiffs_conf = {
	.base_path = "/webroot",
	.partition_label = "webroot",
	.max_files = 5,
	.format_if_mount_failed = false
};

static const spi_bus_config_t hc595_spi_bus_cfg = {
	.mosi_io_num = GPIO_HC595_DATA,
	.miso_io_num = -1,
	.sclk_io_num = GPIO_HC595_CLK,
	.max_transfer_sz = 16,
	.flags = SPICOMMON_BUSFLAG_MASTER
};

static gpio_hc595_t hc595;

static i2c_bus_t i2c_bus;
static i2c_bus_t smbus_i2c_bus;
static smbus_t smbus_bus;

static bq40z50_t bq40z50;

static httpd_t httpd;

prometheus_t prometheus;
prometheus_metric_t metric_simple;
prometheus_metric_t metric_complex;
prometheus_battery_metrics_t battery_metrics;

static volatile bool do_shutdown = false;

static const ethernet_config_t ethernet_cfg = {
	.phy_address = 0,
	.phy_reset_gpio = GPIO_PHY_RESET,
	.mdc_gpio = GPIO_PHY_MDC,
	.mdio_gpio = GPIO_PHY_MDIO
};

button_event_handler_t button_held_event_handler;

static bool on_button_held_event(const button_event_t *event, void *priv) {
	ESP_LOGI(TAG, "Button held for 5s! Shutting down battery pack...");
	bq40z50_shutdown(&bq40z50);

	return true;
}

const button_event_handler_single_user_cfg_t button_held_cfg = {
	.base = {
		.cb = on_button_held_event,
	},
	.single = {
		.button = BUTTON_ENTER,
		.action = BUTTON_ACTION_HOLD,
		.min_hold_duration_ms = 5000,
	}
};

void app_main() {
	ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffs_conf));

	event_bus_init();
	settings_init();
	vendor_init();
	scheduler_init();
	buttons_init();

	ESP_ERROR_CHECK(ethernet_init(&ethernet_cfg));

//	wifi_init();
//	wifi_start_ap();

	ESP_ERROR_CHECK(spi_bus_initialize(SPI_HC595, &hc595_spi_bus_cfg, SPI_DMA_DISABLED));
	ESP_ERROR_CHECK(gpio_hc595_init(&hc595, SPI_HC595, GPIO_HC595_LATCH));
	gpio_hc595_set_level(&hc595, GPIO_HC595_DC_OUT3_OFF, 0);
	gpio_hc595_set_level(&hc595, GPIO_HC595_DC_OUT_TEST, 0);
	gpio_hc595_set_level(&hc595, GPIO_HC595_DC_OUT_OFF, 0);
	gpio_hc595_set_level(&hc595, GPIO_HC595_USB_OUT_OFF, 0);
	gpio_hc595_set_level(&hc595, GPIO_HC595_DC_OUT2_OFF, 0);
	gpio_hc595_set_level(&hc595, GPIO_HC595_DC_OUT1_OFF, 0);

	ESP_ERROR_CHECK(i2c_bus_init(&smbus_i2c_bus, I2C_SMBUS, GPIO_SMBUS_DATA, GPIO_SMBUS_CLK, KHZ(100)));
	smbus_init(&smbus_bus, &smbus_i2c_bus);
	i2c_detect(&smbus_i2c_bus);

	ESP_ERROR_CHECK(i2c_bus_init(&i2c_bus, I2C_I2C, GPIO_I2C_DATA, GPIO_I2C_CLK, KHZ(100)));
	i2c_detect(&i2c_bus);
	display_init(&i2c_bus);
	power_path_early_init(&smbus_bus, &i2c_bus);
	vTaskDelay(pdMS_TO_TICKS(5000));

	ESP_ERROR_CHECK(bq40z50_init(&bq40z50, &smbus_bus, -1));
	battery_gauge_init(&bq40z50.gauge);

	power_path_init(&smbus_bus, &i2c_bus);

	battery_protection_init(&bq40z50);

	buttons_register_single_button_event_handler(&button_held_event_handler, &button_held_cfg);
	buttons_enable_event_handler(&button_held_event_handler);

	ESP_ERROR_CHECK(httpd_init(&httpd, "/webroot", 32));
	website_init(&httpd);
	api_init(&httpd);
	prometheus_init(&prometheus);

	prometheus_battery_metrics_init(&battery_metrics, &bq40z50);
	prometheus_add_battery_metrics(&battery_metrics, &prometheus);
	sensor_install_metrics(&prometheus);
	ESP_ERROR_CHECK(prometheus_register_exporter(&prometheus, &httpd, "/prometheus"));

	display_render_loop();
}
