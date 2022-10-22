#include "ssd1306_oled.h"

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "delay.h"
#include "util.h"

const uint8_t init_sequence[] = {
	0xae, /* display off */
	0x00, /* set low column address */
	0x12, /* set high column address */
	0x40, /* set display start line */
	0xB0, /* set page address */
	0x81, /* set contrast */
	0xff, /* contrast = 128 */
	0xA1, /* set segment remap */
	0xa6, /* normal / reverse */
	0xa8, /* set multiplex ratio */
	0x2f, /* multiplex ratio = 1/48 */
	0xc8, /* set COM scan direction */
	0xd3, /* set display offset */
	0x00, /* display offset = 0 */
	0xd5, /* set clock divider */
	0x80, /* clock speed = ? */
	0xd9, /* set pre-charge period */
	0x21, /* pre-charge period */
	0xda, /* set COM pins */
	0x40,
	0x8d, /* enable charge pump */
	0x14,
	0xaf
};

static esp_err_t send_command(ssd1306_oled_t *oled, uint8_t command) {
	i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(oled->xfers_cmd, sizeof(oled->xfers_cmd));
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (oled->address << 1), true);
	i2c_master_write_byte(cmd, 0x00, true);
	i2c_master_write_byte(cmd, command, true);
	i2c_master_stop(cmd);
	esp_err_t err = i2c_bus_cmd_begin(oled->bus, cmd, pdMS_TO_TICKS(10));
	i2c_cmd_link_delete_static(cmd);
	return err;
}

/*
static esp_err_t send_multibyte_command(ssd1306_oled_t *oled, uint8_t* data, unsigned int len) {
	i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(oled->xfers_cmd, sizeof(oled->xfers_cmd));
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (oled->address << 1), true);
	i2c_master_write_byte(cmd, 0x40, true);
	i2c_master_write(cmd, data, len, I2C_MASTER_ACK);
	i2c_master_stop(cmd);
	esp_err_t err = i2c_bus_cmd_begin(oled->bus, cmd, pdMS_TO_TICKS(10));
	i2c_cmd_link_delete_static(cmd);
	return err;
}
*/

static esp_err_t send_command_list(ssd1306_oled_t *oled, const uint8_t *cmds, unsigned int num_cmds) {
	while (num_cmds--) {
		esp_err_t err = send_command(oled, *cmds++);
		if (err) {
			return err;
		}
	}
	return ESP_OK;
}

static void reset(ssd1306_oled_t *oled) {
	if (oled->reset_gpio >= 0) {
		gpio_set_level(oled->reset_gpio, 0);
		delay_us(10);
		gpio_set_level(oled->reset_gpio, 1);
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

esp_err_t ssd1306_oled_init(ssd1306_oled_t *oled, i2c_bus_t *bus, unsigned int address, int reset_gpio) {
	oled->bus = bus;
	oled->address = address;
	oled->reset_gpio = reset_gpio;
	if (reset_gpio >= 0) {
		gpio_set_direction(reset_gpio, GPIO_MODE_OUTPUT);
	}
	reset(oled);
	esp_err_t err = send_command_list(oled, init_sequence, ARRAY_SIZE(init_sequence));
	if (err) {
		return err;
	}

	// debug, set entire display on
	return send_command(oled, 0xa5);
}
