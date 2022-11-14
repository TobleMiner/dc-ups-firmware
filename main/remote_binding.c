#include "remote_binding.h"

#include "httpd.h"

static void websocket_binding_free_ctx(void *arg) {
	rte_event_handler_t *event_handler = arg;

	ESP_LOGI(LOG_TAG, "Event handler for websocket removed");
	rte_remove_event_handler(event_handler);
}

static esp_err_t websocket_binding_cb(struct httpd_request_ctx* ctx, void* priv) {
	httpd_req_t *req = ctx->req;

	if (req->method == HTTP_GET) {
		rte_event_handler_t *event_handler;
		ws_cb_ctx_t cb_ctx = {
			req->handle,
			httpd_req_to_sockfd(req),
			{ 0 }
		};

		event_handler = rte_register_event_handler_cb(trigger_websocket_message, &cb_ctx, sizeof(cb_ctx));
		if (!event_handler) {
			ESP_LOGE(LOG_TAG, "Failed to register event handler for websocket");
			return httpd_send_error(ctx, HTTPD_500);
		}

		req->sess_ctx = event_handler;
		req->free_ctx = http_websocket_event_free_ctx;

		ESP_LOGI(LOG_TAG, "Event handler for websocket registered");

		return ESP_OK;
	}

	return ESP_OK;
}


static void remote_binding_init(httpd_t *httpd) {
	ESP_ERROR_CHECK(httpd_add_websocket_handler(httpd, "/api/v1/binding", websocket_binding_cb, NULL, 0));
}