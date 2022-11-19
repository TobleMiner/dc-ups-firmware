#pragma once

#include <stddef.h>

#include "list.h"

typedef enum {
	MESSAGE_BUS_MESSAGE_TYPE_SENSOR_MEASUREMENT_UPDATE
} message_bus_message_type_t;

typedef struct message_bus_message {
	unsigned int type;
	void *data;
} message_bus_message_t;

typedef struct message_bus_listener {
	list_head_t list;
	void (*callback)(message_bus_message_t *message);
} message_bus_listener_t;

typedef struct message_bus {
	list_head_t listeners;
} message_bus_t;

void message_bus_init(message_bus_t *bus);
void message_bus_broadcast(message_bus_t *bus, message_bus_message_type_t message_type, void *payload);
void message_bus_subscribe(message_bus_t *bus, message_bus_listener_t *listener);
