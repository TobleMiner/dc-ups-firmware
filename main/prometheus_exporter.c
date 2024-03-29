#include "prometheus_exporter.h"

#include <esp_log.h>

const static char *TAG = "prometheus_exporter";

static void handle_labels(const prometheus_metric_value_t *value, prometheus_metric_t *metric, struct httpd_request_ctx* ctx) {
	unsigned int num_static_labels = value->num_labels;
	unsigned int num_dynamic_labels = 0;
	if (value->get_num_labels) {
		num_dynamic_labels = value->get_num_labels(value, metric);
	}
	unsigned int num_labels = num_static_labels + num_dynamic_labels;
	if (!num_labels) {
		return;
	}

	httpd_response_write_string(ctx, "{");
	for (unsigned int i = 0; i < num_static_labels; i++) {
		const prometheus_label_t *label;

		label = &value->labels[i];
		httpd_response_write_string(ctx, label->name);
		httpd_response_write_string(ctx, "=\"");
		httpd_response_write_string(ctx, label->value);
		httpd_response_write_string(ctx, "\"");
		if (i < num_static_labels - 1 || num_dynamic_labels) {
			httpd_response_write_string(ctx, ",");
		}
	}
	for (unsigned int i = 0; i < num_dynamic_labels; i++) {
		char name[PROMETHEUS_LABEL_MAX_LEN] = { 0 };
		char label[PROMETHEUS_LABEL_MAX_LEN] = { 0 };

		value->get_label(value, metric, i, name, label);
		httpd_response_write_string(ctx, name);
		httpd_response_write_string(ctx, "=\"");
		httpd_response_write_string(ctx, label);
		httpd_response_write_string(ctx, "\"");
		if (i < num_dynamic_labels - 1) {
			httpd_response_write_string(ctx, ",");
		}
	}
	httpd_response_write_string(ctx, "} ");
}

static void handle_value(const prometheus_metric_value_t *value, prometheus_metric_t *metric, struct httpd_request_ctx* ctx) {
	const prometheus_metric_def_t *def = metric->def;
	httpd_response_write_string(ctx, def->name);
	httpd_response_write_string(ctx, " ");
	if (value->num_labels || value->get_num_labels) {
		handle_labels(value, metric, ctx);
	}
	char value_str[PROMETHEUS_VALUE_MAX_LEN] = { 0 };
	value->get_value(value, metric, value_str);
	httpd_response_write_string(ctx, value_str);
	httpd_response_write_string(ctx, "\n");
}

static esp_err_t handle_metric(prometheus_metric_t *metric, struct httpd_request_ctx* ctx) {
	const prometheus_metric_def_t *def = metric->def;
	if (!def->name) {
		ESP_LOGE(TAG, "Missing name on metric");
		return ESP_ERR_INVALID_ARG;
	}

	if (def->help) {
		httpd_response_write_string(ctx, "# HELP ");
		httpd_response_write_string(ctx, def->name);
		httpd_response_write_string(ctx, " ");
		httpd_response_write_string(ctx, def->help);
		httpd_response_write_string(ctx, "\n");
	}

	if (def->type != PROMETHEUS_METRIC_TYPE_UNTYPED) {
		const char *type_name;
		switch (def->type) {
		case PROMETHEUS_METRIC_TYPE_COUNTER:
			type_name = "counter";
			break;
		case PROMETHEUS_METRIC_TYPE_GAUGE:
			type_name = "gauge";
			break;
		default:
			ESP_LOGE(TAG, "Invalid type for metric %s", def->name);
			return ESP_ERR_INVALID_ARG;
		}
		httpd_response_write_string(ctx, "# TYPE ");
		httpd_response_write_string(ctx, def->name);
		httpd_response_write_string(ctx, " ");
		httpd_response_write_string(ctx, type_name);
		httpd_response_write_string(ctx, "\n");
	}

	for (unsigned int i = 0; i < def->num_values; i++) {
		handle_value(&def->values[i], metric, ctx);
	}
	if (def->get_num_values) {
		unsigned int num_values = def->get_num_values(metric);
		for (unsigned int i = 0; i < num_values; i++) {
			prometheus_metric_value_t value;
			memset(&value, 0, sizeof(value));
			def->get_value(metric, i, &value);
			handle_value(&value, metric, ctx);
		}
	}
	return ESP_OK;
}

static esp_err_t request_handler(struct httpd_request_ctx* ctx, void* priv) {
	prometheus_t *prometheus = priv;

	prometheus_metric_t *metric;
	LIST_FOR_EACH_ENTRY(metric, &prometheus->metrics, list) {
		esp_err_t err = handle_metric(metric, ctx);
		if (err) {
			ESP_LOGE(TAG, "Failed to handle metric");
			httpd_send_error(ctx, HTTPD_500);
			return err;
		}
		httpd_response_write_string(ctx, "\n");
	}

	httpd_finalize_response(ctx);
	return ESP_OK;
}

esp_err_t prometheus_register_exporter(prometheus_t *prometheus, httpd_t *httpd, char *path) {
	return httpd_add_get_handler(httpd, path, request_handler, prometheus, false, 0);
}
