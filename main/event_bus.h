#pragma once

#include "list.h"

typedef void (*eventbus_notify_cb_f)(void *priv, void *data);

typedef struct event_bus_hamdler {
	struct list_head list;
	const char *topic;
	eventbus_notify_cb_f notify_cb;
	void *priv;
} event_bus_handler_t;

void event_bus_init(void);
void event_bus_notify(const char *topic, void *data);
void event_bus_subscribe(event_bus_handler_t *handler, const char *topic, eventbus_notify_cb_f notify_cb, void *priv);
