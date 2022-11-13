#include "website.h"

#include <stdio.h>
#include <stdlib.h>

#include <esp_err.h>
#include <esp_log.h>

#include "power_path.h"
#include "sensor.h"

const char *TAG = "web";

static const char *get_sensor_name(const char *name) {
	if (name && !strcmp(name, "ina_dc_out")) {
		if (power_path_is_running_on_battery()) {
			return "ina_dc_out_step_up";
		} else {
			return "ina_dc_out_passthrough";
		}
	}

	if (name && !strcmp(name, "lm75_dc_in")) {
		return "lm75_charger";
	}

	return name;
}

static esp_err_t measure_sensor(struct httpd_slice_ctx *slice_ctx, const char *sensor_name_variable, sensor_measurement_type_t type, long *res) {
	const char *sensor_name = get_sensor_name(slice_scope_get_variable(slice_ctx, sensor_name_variable));
	sensor_t *sensor = sensor_find_by_name(sensor_name);
	if (!sensor) {
		ESP_LOGW(TAG, "Failed to find sensor %s", sensor_name ? sensor_name : "(null)");
		return ESP_FAIL;
	}
	return sensor_measure(sensor, type, 0, res);
}

static esp_err_t status_panel_voltage_cb(void* ctx, void* priv, struct templ_slice* slice) {
	struct httpd_slice_ctx *slice_ctx = ctx;
	long voltage_mv;
	esp_err_t err = measure_sensor(slice_ctx, "ina", SENSOR_TYPE_VOLTAGE, &voltage_mv);
	if (err) {
		return httpd_response_write_string(slice_ctx->req_ctx, "Measurement failed");
	}

	ESP_LOGI(TAG, "INA voltage: %ld mV", voltage_mv);

	char strbuf[32];
	snprintf(strbuf, sizeof(strbuf), "%ld.%02ld V", voltage_mv / 1000L, (voltage_mv % 1000L) / 10);
	return httpd_response_write_string(slice_ctx->req_ctx, strbuf);
}

static esp_err_t status_panel_current_cb(void* ctx, void* priv, struct templ_slice* slice) {
	struct httpd_slice_ctx *slice_ctx = ctx;
	long current_ma;
	esp_err_t err = measure_sensor(slice_ctx, "ina", SENSOR_TYPE_CURRENT, &current_ma);
	if (err) {
		return httpd_response_write_string(slice_ctx->req_ctx, "Measurement failed");
	}

	char strbuf[32];
	snprintf(strbuf, sizeof(strbuf), "%s%ld.%02ld A", current_ma < 0 ? "-" : "", ABS(current_ma) / 1000L, (ABS(current_ma) % 1000L) / 10);
	return httpd_response_write_string(slice_ctx->req_ctx, strbuf);
}

static esp_err_t status_panel_power_cb(void* ctx, void* priv, struct templ_slice* slice) {
	struct httpd_slice_ctx *slice_ctx = ctx;
	long power_mw;
	esp_err_t err = measure_sensor(slice_ctx, "ina", SENSOR_TYPE_POWER, &power_mw);
	if (err) {
		return httpd_response_write_string(slice_ctx->req_ctx, "Measurement failed");
	}

	char strbuf[32];
	snprintf(strbuf, sizeof(strbuf), "%s%ld.%02ld W", power_mw < 0 ? "-" : "", ABS(power_mw) / 1000L, (ABS(power_mw) % 1000L) / 10);
	return httpd_response_write_string(slice_ctx->req_ctx, strbuf);
}

static esp_err_t status_panel_temperature_cb(void* ctx, void* priv, struct templ_slice* slice) {
	struct httpd_slice_ctx *slice_ctx = ctx;
	long temperature_mdegc;
	esp_err_t err = measure_sensor(slice_ctx, "lm75", SENSOR_TYPE_TEMPERATURE, &temperature_mdegc);
	if (err) {
		return httpd_response_write_string(slice_ctx->req_ctx, "Measurement failed");
	}

	char strbuf[32];
	snprintf(strbuf, sizeof(strbuf), "%s%ld.%02ld C", temperature_mdegc < 0 ? "-" : "", ABS(temperature_mdegc) / 1000L, (ABS(temperature_mdegc) % 1000L) / 10);
	return httpd_response_write_string(slice_ctx->req_ctx, strbuf);
}

static esp_err_t dcout_voltage_cb(void* ctx, void* priv, struct templ_slice* slice) {
	struct httpd_slice_ctx *slice_ctx = ctx;
	unsigned int voltage_mv = power_path_get_dc_output_voltage_mv();
	char strbuf[32];
	snprintf(strbuf, sizeof(strbuf), "%u.%01u V", voltage_mv / 1000, (voltage_mv % 1000) / 100);
	return httpd_response_write_string(slice_ctx->req_ctx, strbuf);
}

static esp_err_t dcout_enabled_cb(void* ctx, void* priv, struct templ_slice* slice) {
	struct httpd_slice_ctx *slice_ctx = ctx;
	const char *output_id_str = slice_scope_get_variable(slice_ctx, "output");
	if (!output_id_str) {
		ESP_LOGE(TAG, "Missing output variable in slice scope");
		return ESP_FAIL;
	}
	int output_id = atoi(output_id_str);
	if (output_id < 1 || output_id > 3) {
		ESP_LOGE(TAG, "Invalid output ID");
		return ESP_FAIL;
	}
	bool output_enabled = power_path_is_dc_output_enabled(output_id - 1);
	if (output_enabled) {
		return httpd_response_write_string(slice_ctx->req_ctx, "checked");
	}
	return ESP_OK;
}

void website_init(httpd_t *httpd) {
	/* Templates */
	ESP_ERROR_CHECK(httpd_add_template(httpd, "status_panel.voltage", status_panel_voltage_cb, NULL));
	ESP_ERROR_CHECK(httpd_add_template(httpd, "status_panel.current", status_panel_current_cb, NULL));
	ESP_ERROR_CHECK(httpd_add_template(httpd, "status_panel.power", status_panel_power_cb, NULL));
	ESP_ERROR_CHECK(httpd_add_template(httpd, "status_panel.temperature", status_panel_temperature_cb, NULL));
	ESP_ERROR_CHECK(httpd_add_template(httpd, "dcout.voltage", dcout_voltage_cb, NULL));
	ESP_ERROR_CHECK(httpd_add_template(httpd, "dcout.enabled", dcout_enabled_cb, NULL));
	/* Index */
	ESP_ERROR_CHECK(httpd_add_redirect(httpd, "/", "/index.thtml"));
	/* Static files */
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/binding.js"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/bootstrap.bundle.min.js"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/bootstrap.min.css"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/jquery-1.8.3.min.js"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/index.thtml"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/dcin.thtml"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/dcout.thtml"));
/*
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/usbout.thtml"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/battery.thtml"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/network.thtml"));
*/
}