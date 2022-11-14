#pragma once

#include <stddef.h>

#include "list.h"

typedef struct message_bus_message {
	void *data;
	size_t data_len;
} message_bus_message_t;

typedef struct message_bus_listener {
	list_head_t list;
	void (*callback)(message_bus_message_t *message);
} message_bus_listener_t;

typedef struct message_bus {
	list_head_t listeners;
} message_bus_t;