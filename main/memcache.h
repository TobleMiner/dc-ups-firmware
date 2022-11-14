#pragma once

#include "kvstore.h"

typedef kvstore_entry_t memcache_entry_t;
typedef kvstore_listener_t memcache_listener_t;

esp_err_t memcache_init(void);
esp_err_t memcache_add_entry_int(memcache_entry_t *entry, const char *key, bool persistent, long long default_value);
esp_err_t memcache_add_entry_string(memcache_entry_t *entry, const char *key, bool persistent, const char *default_value);
esp_err_t memcache_update_entry_int(memcache_entry_t *entry, long long value, memcache_listener_t *exclude);
esp_err_t memcache_update_entry_string(memcache_entry_t *entry, const char *value, memcache_listener_t *exclude);
memcache_entry_t *memcache_find_entry_by_key(const char *key);
long long memcache_get_int(memcache_entry_t *entry);
char *memcache_get_string(memcache_entry_t *entry);
void memcache_subscribe(memcache_listener_t *listener);
