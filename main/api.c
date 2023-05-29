#include "api.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "power_path.h"

static esp_err_t http_get_set_input_current_limit(struct httpd_request_ctx* ctx, void* priv) {
	ssize_t param_len;
	char* current_limit_str;
	unsigned long current_limit_ma;
	int err;

	if((param_len = httpd_query_string_get_param(ctx, "current_ma", &current_limit_str)) <= 0) {
		return httpd_send_error(ctx, HTTPD_400);
	}

	errno = 0;
	current_limit_ma = strtoul(current_limit_str, NULL, 10);
	if (current_limit_ma == ULONG_MAX || errno) {
		return httpd_send_error(ctx, HTTPD_400);
	}

	power_path_set_input_current_limit(current_limit_ma);

	httpd_finalize_response(ctx);
	return ESP_OK;
}

void api_init(httpd_t *httpd) {
	ESP_ERROR_CHECK(httpd_add_get_handler(httpd, "/api/v1/set_input_current_limit", http_get_set_input_current_limit, NULL, 1, "current_ma"));
}
