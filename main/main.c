#include <driver/gpio.h>
#include <driver/i2c.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_spiffs.h>
#include <freertos/FreeRTOS.h>

#include "api.h"
#include "bq40z50_gauge.h"
#include "buttons.h"
#include "display.h"
#include "ethernet.h"
#include "event_bus.h"
#include "font_3x5.h"
#include "gpio_hc595.h"
#include "httpd.h"
#include "i2c_bus.h"
#include "ina219.h"
#include "lm75.h"
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

#define LM75_CHARGER	0
#define LM75_DC_OUT	1
#define LM75_USB_OUT	2

static lm75_t lm75[3];
static const char *lm75_names[3] = { "lm75_charger", "lm75_dc_out", "lm75_usb_out" };
static unsigned int lm75_address[3] = { 0x48, 0x49, 0x4a };

static bq40z50_t bq40z50;

#define INA_DC_IN		0
#define INA_DC_OUT_PASSTHROUGH	1
#define INA_DC_OUT_STEP_UP	2
#define INA_USB_OUT		3

static ina219_t ina[4];
static unsigned int ina_address[4] = { 0x40, 0x41, 0x42, 0x43 };
static const char *ina_names[4] = { "ina_dc_in", "ina_dc_out_passthrough", "ina_dc_out_step_up", "ina_usb_out" };

/*
static ssd1306_oled_t oled;
static fb_t fb;
*/

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

/*
static void button_pressed(void *_) {
	do_shutdown = true;
}
*/

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
//	ESP_ERROR_CHECK(gpio_install_isr_service(0));

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
/*
	for (int i = 0; i < ARRAY_SIZE(lm75); i++) {
		lm75_init(&lm75[i], &i2c_bus, lm75_address[i], lm75_names[i]);
		int32_t temp_mdegc = 0;
		esp_err_t err = lm75_read_temperature_mdegc(&lm75[i], &temp_mdegc);
		if (err) {
			ESP_LOGI(TAG, "Temperature readout failed on sensor %d: %d", i, err);
		} else {
			ESP_LOGI(TAG, "Sensor %d: %.2f°C", i, (float)temp_mdegc / 1000.0f);
		}
	}
*/
	display_init(&i2c_bus);
/*
	ESP_ERROR_CHECK(ssd1306_oled_init(&oled, &i2c_bus, 0x3c, GPIO_OLED_RESET));
	fb_init(&fb);
*/

	ESP_ERROR_CHECK(bq40z50_init(&bq40z50, &smbus_bus, -1));
	battery_gauge_init(&bq40z50.gauge);

	buttons_register_single_button_event_handler(&button_held_event_handler, &button_held_cfg);
	buttons_enable_event_handler(&button_held_event_handler);
/*
	gpio_set_direction(GPIO_BUTTON, GPIO_MODE_INPUT);
	gpio_set_intr_type(GPIO_BUTTON, GPIO_INTR_NEGEDGE);
	ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_BUTTON, button_pressed, NULL));
*/

	power_path_init(&smbus_bus, &i2c_bus);
/*
	for (int i = 0; i < ARRAY_SIZE(ina); i++) {
		ESP_ERROR_CHECK(ina219_init(&ina[i], &smbus_bus, ina_address[i], 10, ina_names[i]));
		ESP_ERROR_CHECK(ina219_set_shunt_voltage_range(&ina[i], INA219_PGA_CURRENT_GAIN_80MV));
		unsigned int bus_voltage_mv;
		long current_ua, shunt_voltage_uv, power_uw;
		ESP_ERROR_CHECK(ina219_read_shunt_voltage_uv(&ina[i], &shunt_voltage_uv));
		ESP_ERROR_CHECK(ina219_read_bus_voltage_mv(&ina[i], &bus_voltage_mv));
		ESP_ERROR_CHECK(ina219_read_current_ua(&ina[i], &current_ua));
		ESP_ERROR_CHECK(ina219_read_power_uw(&ina[i], &power_uw));
		ESP_LOGI(TAG, "INA %d: %umV @ %ldmA (Ushunt: %lduV, %ldmW)", i, bus_voltage_mv, current_ua / 1000L, shunt_voltage_uv, power_uw / 1000L);
	}
*/
	ESP_ERROR_CHECK(httpd_init(&httpd, "/webroot", 32));
	website_init(&httpd);
	api_init(&httpd);
	prometheus_init(&prometheus);
/*
	prometheus_metric_init(&metric_simple, &test_metric_def, NULL);
	prometheus_add_metric(&prometheus, &metric_simple);
	prometheus_metric_init(&metric_complex, &complex_test_metric_def, NULL);
	prometheus_add_metric(&prometheus, &metric_complex);
*/
	prometheus_battery_metrics_init(&battery_metrics, &bq40z50);
	prometheus_add_battery_metrics(&battery_metrics, &prometheus);
	sensor_install_metrics(&prometheus);
	ESP_ERROR_CHECK(prometheus_register_exporter(&prometheus, &httpd, "/prometheus"));

	unsigned int toggle_gpios[] = { GPIO_HC595_USB_OUT_OFF, GPIO_HC595_DC_OUT1_OFF, GPIO_HC595_DC_OUT2_OFF, GPIO_HC595_DC_OUT3_OFF, GPIO_HC595_DC_OUT_OFF };
	unsigned int gpio_idx = 0;
	unsigned int pixel = 0;
	unsigned int last_pixel = pixel;

	display_render_loop();
/*
	while (1) {
		ESP_LOGI(TAG, "Toggling GPIO %u", gpio_idx);
		unsigned int gpio = toggle_gpios[gpio_idx++];
		gpio_idx %= ARRAY_SIZE(toggle_gpios);
//		ESP_ERROR_CHECK(gpio_hc595_set_level(&hc595, gpio, 1));
//		vTaskDelay(pdMS_TO_TICKS(1000));
//		ESP_ERROR_CHECK(gpio_hc595_set_level(&hc595, gpio, 0));
//		vTaskDelay(pdMS_TO_TICKS(1000));

		unsigned int charging_current_ma, charging_voltage_mv;
		ESP_ERROR_CHECK(bq40z50_get_charging_current_ma(&bq40z50, &charging_current_ma));
		ESP_ERROR_CHECK(bq40z50_get_charging_voltage_mv(&bq40z50, &charging_voltage_mv));
		ESP_LOGI(TAG, "Charging parameters: %umA, %umV", charging_current_ma, charging_voltage_mv);
		if (charging_current_ma > 512) {
			charging_current_ma = 512;
		}
		charging_current_ma += 32;
		charging_current_ma = 1000;
		ESP_ERROR_CHECK(bq24715_set_charge_current(&bq24715, charging_current_ma));

		unsigned int cell_voltage1, cell_voltage2;
		unsigned int state_of_charge;
		int current_ma;
		ESP_ERROR_CHECK(bq40z50_get_cell_voltage_mv(&bq40z50, BQ40Z50_CELL_1, &cell_voltage1));
		ESP_ERROR_CHECK(bq40z50_get_cell_voltage_mv(&bq40z50, BQ40Z50_CELL_2, &cell_voltage2));
		ESP_ERROR_CHECK(bq40z50_get_state_of_charge_percent(&bq40z50, &state_of_charge));
		ESP_LOGI(TAG, "Cell voltages: %umV, %umV", cell_voltage1, cell_voltage2);
		ESP_ERROR_CHECK(bq40z50_get_current_ma(&bq40z50, &current_ma));
		if (current_ma > 0) {
			ESP_LOGI(TAG, "Charging at %dmA", current_ma);
		} else {
			ESP_LOGI(TAG, "Discharging at %dmA", -current_ma);
		}
		char display_str[16 + 1];

		// Battery stats
		snprintf(display_str, sizeof(display_str), "SOC: %03u%%", state_of_charge);
		font_3x5_render_string(display_str, &fb, 0, 0);
		font_3x5_render_string("Cell voltages", &fb, 6, 6);
		snprintf(display_str, sizeof(display_str), "%04umV  %04umV", cell_voltage1, cell_voltage2);
		font_3x5_render_string(display_str, &fb, 3, 12);

		// Output stats
		long power_passthrough_uw;
		long power_step_up_uw;
		long power_usb_uw;
		ESP_ERROR_CHECK(ina219_read_power_uw(&ina[INA_DC_OUT_PASSTHROUGH], &power_passthrough_uw));
		ESP_ERROR_CHECK(ina219_read_power_uw(&ina[INA_DC_OUT_STEP_UP], &power_step_up_uw));
		ESP_ERROR_CHECK(ina219_read_power_uw(&ina[INA_USB_OUT], &power_usb_uw));
		long power_total_uw = power_passthrough_uw + power_step_up_uw + power_usb_uw;
		snprintf(display_str, sizeof(display_str), "Power: %02.02fW", (float)power_total_uw / 1000000.0f);
		font_3x5_render_string(display_str, &fb, 3, 18);

		// System status
		const char *system_status = "   IDLE    ";
		if (current_ma > 0) {
			system_status = " CHARGING  ";
		} else if (current_ma < 0) {
			system_status = "DISCHARGING";
		}
		font_3x5_render_string(system_status, &fb, 9, 24);
		font_3x5_render_string("Battery current", &fb, 3, 30);
		snprintf(display_str, sizeof(display_str), "%04dmA ", current_ma);
		font_3x5_render_string(display_str, &fb, 17, 36);

		int32_t temp_charger_mdegc = 0;
		lm75_read_temperature_mdegc(&lm75[LM75_CHARGER], &temp_charger_mdegc);
		int32_t temp_dc_out_mdegc = 0;
		lm75_read_temperature_mdegc(&lm75[LM75_DC_OUT], &temp_dc_out_mdegc);
		int32_t temp_usb_out_mdegc = 0;
		lm75_read_temperature_mdegc(&lm75[LM75_USB_OUT], &temp_usb_out_mdegc);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
		snprintf(display_str, sizeof(display_str), "%02ldC %02ldC %02ldC ", temp_charger_mdegc / 1000, temp_dc_out_mdegc / 1000, temp_usb_out_mdegc / 1000);
#pragma GCC diagnostic pop
		font_3x5_render_string(display_str, &fb, 9, 42);

		ESP_ERROR_CHECK(ssd1306_oled_render_fb(&oled, &fb));
		vTaskDelay(pdMS_TO_TICKS(10000));
*/
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
/*
		if (do_shutdown) {
			ESP_LOGI(TAG, "Button pressed! Shutting down battery pack...");
			bq40z50_shutdown(&bq40z50);
			do_shutdown = false;
		}
	}
*/
}
