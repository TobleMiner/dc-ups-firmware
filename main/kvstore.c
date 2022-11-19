#include "kvstore.h"

#include <stdlib.h>
#include <string.h>

#include <esp_log.h>

static const char *TAG = "kvstore";

esp_err_t kvstore_init(kvstore_t *store, const char *nvs_namespace) {
	memset(store, 0, sizeof(*store));
	if (nvs_namespace) {
		esp_err_t err = nvs_flash_init();
		if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND ||
		    err == ESP_ERR_NVS_INVALID_STATE) {
			ESP_LOGW(TAG, "NVS structure invalid, reformatting");
			err = nvs_flash_erase();
			if (err) {
				return err;
			}
			err = nvs_flash_init();
		}
		if (err) {
			ESP_LOGE(TAG, "Failed to initialize NVS");
			return err;
		}

		err = nvs_open(nvs_namespace, NVS_READWRITE, &store->nvs);
		if (err) {
			ESP_LOGE(TAG, "Failed to open NVS namespace");
			return err;
		}
	}
	INIT_LIST_HEAD(store->entries);
	INIT_LIST_HEAD(store->listeners);
	store->lock = xSemaphoreCreateRecursiveMutexStatic(&store->lock_buffer);
	return ESP_OK;
}

void kvstore_entry_init_const_string(kvstore_entry_t *entry, const char *key, bool persistent, const char *default_value) {
	entry->key = key;
	entry->type = KVSTORE_ENTRY_TYPE_INT;
	entry->persistent = persistent;
	entry->blob.ptr = default_value;
	entry->blob.on_heap = false;
}

void kvstore_entry_init_int(kvstore_entry_t *entry, const char *key, bool persistent, long long default_value) {
	entry->key = key;
	entry->type = KVSTORE_ENTRY_TYPE_INT;
	entry->persistent = persistent;
	entry->value = default_value;
}

static void add_entry(kvstore_t *store, kvstore_entry_t *entry) {
	INIT_LIST_HEAD(entry->list);
	LIST_APPEND(&entry->list, &store->entries);
}

static esp_err_t kvstore_add_entry_(kvstore_t *store, kvstore_entry_t *entry) {
	if (entry->persistent) {
		if (!store->nvs) {
			ESP_LOGE(TAG, "Can not have persistent entry in kvstore without backing storage");
			return ESP_ERR_INVALID_ARG;
		}
		switch (entry->type) {
		case (KVSTORE_ENTRY_TYPE_INT): {
			int64_t value;
			esp_err_t err = nvs_get_i64(store->nvs, entry->key, &value);
			if (err == ESP_ERR_NVS_NOT_FOUND) {
				add_entry(store, entry);
				return ESP_OK;
			}
			if (err) {
				return err;
			}
			entry->value = value;
			break;
		}
		case (KVSTORE_ENTRY_TYPE_STRING): {
			size_t str_len;
			esp_err_t err = nvs_get_str(store->nvs, entry->key, NULL, &str_len);
			if (err == ESP_ERR_NVS_NOT_FOUND) {
				add_entry(store, entry);
				return ESP_OK;
			}
			if (err) {
				return err;
			}
			char *data = malloc(str_len);
			if (!data) {
				ESP_LOGE(TAG, "Failed to add entry, out of memory");
				return ESP_ERR_NO_MEM;
			}
			err = nvs_get_str(store->nvs, entry->key, data, &str_len);
			if (err) {
				free(data);
				return err;
			}
			entry->blob.ptr = data;
			entry->blob.on_heap = true;
		}
		}
	}
	add_entry(store, entry);
	return ESP_OK;
}

esp_err_t kvstore_add_entry(kvstore_t *store, kvstore_entry_t *entry) {
	xSemaphoreTakeRecursive(store->lock, portMAX_DELAY);
	esp_err_t err = kvstore_add_entry_(store, entry);
	xSemaphoreGiveRecursive(store->lock);
	return err;
}

static void notify_listeners(const kvstore_t *store, kvstore_entry_t *entry, const kvstore_listener_t *exclude) {
	kvstore_listener_t *listener;
	LIST_FOR_EACH_ENTRY(listener, &store->listeners, list) {
		if (listener == exclude) {
			continue;
		}
		if (listener->filter && strcmp(listener->filter, entry->key)) {
			continue;
		}
		listener->callback(entry);
	}
}

static esp_err_t validate_update(kvstore_t *store, kvstore_entry_t *entry) {
	if (entry->persistent && !store->nvs) {
		/* Can not persist entry if NVS has not been configured */
		ESP_LOGE(TAG, "Can not update persistent entry in kvstore without backing storage");
		return ESP_ERR_INVALID_ARG;
	}

	return ESP_OK;
}

esp_err_t kvstore_update_entry_string_(kvstore_t *store, kvstore_entry_t *entry,
				       const char *str, const kvstore_listener_t *exclude) {
	esp_err_t err = validate_update(store, entry);
	if (err) {
		return err;
	}

	if (entry->blob.ptr) {
		/* No change in data, early return */
		if (strcmp(entry->blob.ptr, str) == 0) {
			return ESP_OK;
		}
	}

	char *str_heap = strdup(str);
	if (!str_heap) {
		ESP_LOGE(TAG, "Failed to update string value, out of memory");
		return ESP_ERR_NO_MEM;
	}
	if (entry->persistent) {
		/* Destroy current NVS state only after strdup() was successful */
		err = nvs_set_str(store->nvs, entry->key, str);
		if (err) {
			free(str_heap);
			return err;
		}
	}
	/* Free old entry if it was allocated on heap */
	if (entry->blob.on_heap) {
		free((void *)entry->blob.ptr);
	}
	/* Update entry with new value */
	entry->blob.ptr = str_heap;
	entry->blob.on_heap = true;

	/* Notify all listeners about the change in value */
	if (entry->on_change) {
		entry->on_change(entry);
	}
	notify_listeners(store, entry, exclude);
	return ESP_OK;
}

esp_err_t kvstore_update_entry_string(kvstore_t *store, kvstore_entry_t *entry,
				       const char *str, const kvstore_listener_t *exclude) {
	xSemaphoreTakeRecursive(store->lock, portMAX_DELAY);
	esp_err_t err = kvstore_update_entry_string(store, entry, str, exclude);
	xSemaphoreGiveRecursive(store->lock);
	return err;
}

esp_err_t kvstore_update_entry_int_(kvstore_t *store, kvstore_entry_t *entry,
				    long long val, const kvstore_listener_t *exclude) {
	esp_err_t err = validate_update(store, entry);
	if (err) {
		return err;
	}

	if (entry->value == val) {
		return ESP_OK;
	}

	if (entry->persistent) {
		err = nvs_set_i64(store->nvs, entry->key, val);
		if (err) {
			return err;
		}
	}
	entry->value = val;
	if (entry->on_change) {
		entry->on_change(entry);
	}
	notify_listeners(store, entry, exclude);
	return ESP_OK;
}

esp_err_t kvstore_update_entry_int(kvstore_t *store, kvstore_entry_t *entry,
				   long long val, const kvstore_listener_t *exclude) {
	xSemaphoreTakeRecursive(store->lock, portMAX_DELAY);
	esp_err_t err = kvstore_update_entry_int_(store, entry, val, exclude);
	xSemaphoreGiveRecursive(store->lock);
	return err;
}

kvstore_entry_t *kvstore_find_entry_by_key(kvstore_t *store, const char *key) {
	kvstore_entry_t *entry;
	LIST_FOR_EACH_ENTRY(entry, &store->entries, list) {
		if (strcmp(entry->key, key) == 0) {
			return entry;
		}
	}

	return NULL;
}

long long kvstore_get_int(kvstore_t *store, kvstore_entry_t *entry) {
	xSemaphoreTakeRecursive(store->lock, portMAX_DELAY);
	long long value = entry->value;
	xSemaphoreGiveRecursive(store->lock);
	return value;
}

char *kvstore_get_string(kvstore_t *store, kvstore_entry_t *entry) {
	xSemaphoreTakeRecursive(store->lock, portMAX_DELAY);
	char *value = strdup(entry->blob.ptr);
	xSemaphoreGiveRecursive(store->lock);
	return value;
}

void kvstore_subscribe(kvstore_t *store, kvstore_listener_t *listener) {
	INIT_LIST_HEAD(listener->list);
	xSemaphoreTakeRecursive(store->lock, portMAX_DELAY);
	LIST_APPEND(&listener->list, &store->listeners);
	xSemaphoreGiveRecursive(store->lock);
}
