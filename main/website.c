#include "website.h"

#include <esp_err.h>
#include <esp_log.h>

const char *TAG = "web";

static esp_err_t status_panel_voltage_cb(void* ctx, void* priv, struct templ_slice* slice) {
	struct httpd_slice_ctx *slice_ctx = ctx;
	const char *value = slice_scope_get_variable(slice_ctx, "status_panel_id");

	if (value) {
		return httpd_response_write(slice_ctx->req_ctx, value, strlen(value));
	}

	ESP_LOGW(TAG, "Invalid status_panel_id");
	return ESP_ERR_INVALID_ARG;
}

void website_init(httpd_t *httpd) {
	/* Templates */
	ESP_ERROR_CHECK(httpd_add_template(httpd, "status_panel.voltage", status_panel_voltage_cb, NULL));
	/* Index */
	ESP_ERROR_CHECK(httpd_add_redirect(httpd, "/", "/index.thtml"));
	/* Static files */
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/binding.js"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/bootstrap.bundle.min.js"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/bootstrap.min.css"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/jquery-1.8.3.min.js"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/index.thtml"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/dcin.thtml"));
/*
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/dcout.thtml"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/usbout.thtml"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/battery.thtml"));
	ESP_ERROR_CHECK(httpd_add_static_path(httpd, "/webroot/network.thtml"));
*/
}