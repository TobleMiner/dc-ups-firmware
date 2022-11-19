#include "message_bus.h"

void message_bus_init(message_bus_t *bus) {
	INIT_LIST_HEAD(bus->listeners);
}

void message_bus_broadcast(message_bus_t *bus, message_bus_message_type_t message_type, void *payload) {
	message_bus_message_t msg = {
		.type = message_type,
		.data = payload
	};
	message_bus_listener_t *listener;
	LIST_FOR_EACH_ENTRY(listener, &bus->listeners, list) {
		listener->callback(&msg);
	}
}

void message_bus_subscribe(message_bus_t *bus, message_bus_listener_t *listener) {
	INIT_LIST_HEAD(listener->list);
	LIST_APPEND(&listener->list, &bus->listeners);
}
