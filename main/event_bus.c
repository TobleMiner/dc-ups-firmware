#include "event_bus.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static DECLARE_LIST_HEAD(event_bus_handlers);
static StaticSemaphore_t event_bus_lock_buffer;
static SemaphoreHandle_t event_bus_lock;

void event_bus_init(void) {
	event_bus_lock = xSemaphoreCreateRecursiveMutexStatic(&event_bus_lock_buffer);
}

void event_bus_subscribe(event_bus_handler_t *handler, const char *topic, eventbus_notify_cb_f notify_cb, void *priv) {
	INIT_LIST_HEAD(handler->list);
	handler->topic = topic;
	handler->notify_cb = notify_cb;
	handler->priv = priv;
	xSemaphoreTakeRecursive(event_bus_lock, portMAX_DELAY);
	LIST_APPEND(&handler->list, &event_bus_handlers);
	xSemaphoreGiveRecursive(event_bus_lock);
}

void event_bus_notify(const char *topic, void *data) {
	event_bus_handler_t *handler;

	xSemaphoreTakeRecursive(event_bus_lock, portMAX_DELAY);
	LIST_FOR_EACH_ENTRY(handler, &event_bus_handlers, list) {
		if (!strcmp(topic, handler->topic)) {
			handler->notify_cb(handler->priv, data);
		}
	}
	xSemaphoreGiveRecursive(event_bus_lock);
}
