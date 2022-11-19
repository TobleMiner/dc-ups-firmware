#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs_flash.h>

#include "list.h"

typedef enum {
	KVSTORE_ENTRY_TYPE_INT,
	KVSTORE_ENTRY_TYPE_STRING
} kvstore_entry_type_t;

typedef struct kvstore_entry {
	const char *key;
	kvstore_entry_type_t type;
	union {
		long long value;
		struct {
			const void *ptr;
			bool on_heap;
		} blob;
	};
	list_head_t list;
	bool persistent;
	void (*on_change)(kvstore_entry_t *entry);
} kvstore_entry_t;

typedef struct kvstore_listener {
	list_head_t list;
	const char *filter;
	void (*callback)(kvstore_entry_t *entry);
} kvstore_listener_t;

typedef struct kvstore {
	nvs_handle_t nvs;
	list_head_t entries;
	list_head_t listeners;
	StaticSemaphore_t lock_buffer;
	SemaphoreHandle_t lock;
} kvstore_t;

esp_err_t kvstore_init(kvstore_t *store, const char *nvs_namespace);
void kvstore_entry_init_const_string(kvstore_entry_t *entry, const char *key, bool persistent, const char *default_value);
void kvstore_entry_init_int(kvstore_entry_t *entry, const char *key, bool persistent, long long default_value);
esp_err_t kvstore_add_entry(kvstore_t *store, kvstore_entry_t *entry);
esp_err_t kvstore_update_entry_int(kvstore_t *store, kvstore_entry_t *entry,
				   long long val, const kvstore_listener_t *exclude);
esp_err_t kvstore_update_entry_string(kvstore_t *store, kvstore_entry_t *entry,
				      const char *str, const kvstore_listener_t *exclude);
kvstore_entry_t *kvstore_find_entry_by_key(kvstore_t *store, const char *key);
long long kvstore_get_int(kvstore_t *store, kvstore_entry_t *entry);
char *kvstore_get_string(kvstore_t *store, kvstore_entry_t *entry);
void kvstore_subscribe(kvstore_t *store, kvstore_listener_t *listener);
