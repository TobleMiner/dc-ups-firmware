#include "memcache.h"

#define MEMCACHE_NAMESPACE	"memcache"

static kvstore_t memcache_kvstore;

esp_err_t memcache_init(void) {
	return kvstore_init(&memcache_kvstore, MEMCACHE_NAMESPACE);
}

esp_err_t memcache_add_entry_int(memcache_entry_t *entry, const char *key, bool persistent, long long default_value) {
	kvstore_entry_init_int(entry, key, persistent, default_value);
	return kvstore_add_entry(&memcache_kvstore, entry);
}

esp_err_t memcache_add_entry_string(memcache_entry_t *entry, const char *key, bool persistent, const char *default_value) {
	kvstore_entry_init_const_string(entry, key, persistent, default_value);
	return kvstore_add_entry(&memcache_kvstore, entry);
}

esp_err_t memcache_update_entry_int(memcache_entry_t *entry, long long value, memcache_listener_t *exclude) {
	return kvstore_update_entry_int(&memcache_kvstore, entry, value, exclude);
}

esp_err_t memcache_update_entry_string(memcache_entry_t *entry, const char *value, memcache_listener_t *exclude) {
	return kvstore_update_entry_string(&memcache_kvstore, entry, value, exclude);
}

memcache_entry_t *memcache_find_entry_by_key(const char *key) {
	return kvstore_find_entry_by_key(&memcache_kvstore, key);
}

long long memcache_get_int(memcache_entry_t *entry) {
	return kvstore_get_int(&memcache_kvstore, entry);
}

char *memcache_get_string(memcache_entry_t *entry) {
	return kvstore_get_string(&memcache_kvstore, entry);
}

void memcache_subscribe(memcache_listener_t *listener) {
	kvstore_subscribe(&memcache_kvstore, listener);
}
