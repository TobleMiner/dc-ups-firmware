#include "callcache.h"

#include <stdlib.h>

#include <esp_log.h>
#include <esp_timer.h>

#include "util.h"

static const char *TAG = "callcache";

static size_t callcache_serialize_write_uint_raw(unsigned char **ptr, unsigned long long val) {
	size_t len = 0;
	if (ptr) {
		unsigned char *serialize_ptr = *ptr;
		do {
			unsigned char val7 = val & 0x7f;
			val >>= 7;
			if (!val) {
				val7 |= 0x80;
			}
			*serialize_ptr++ = val7;
			len++;
		} while (val);
		*ptr = serialize_ptr;
	} else {
		do {
			val >>= 7;
			len++;
		} while (val);
	}
	return len;
}

static unsigned long long callcache_deserialize_read_uint(unsigned char **ptr) {
	unsigned char *deserialize_ptr = *ptr;
	unsigned long long value = 0;
	unsigned int shift = 0;
	do {
		value |= ((unsigned long long)(*deserialize_ptr & 0x7f)) << shift;
		shift += 7;
	} while (!(*deserialize_ptr++ & 0x80));
	*ptr = deserialize_ptr;
	return value;
}

static size_t callcache_serialize_write_uint(unsigned char **ptr, unsigned long long val) {
	size_t len = callcache_serialize_write_uint_raw(ptr, CALLCACHE_CALL_ARG_TYPE_UINT);
	len += callcache_serialize_write_uint_raw(ptr, val);
	return len;
}

static size_t callcache_serialize_write_int(unsigned char **ptr, long long val) {
	unsigned long long uval = val;
	size_t len = callcache_serialize_write_uint_raw(ptr, CALLCACHE_CALL_ARG_TYPE_INT);
	len += callcache_serialize_write_uint_raw(ptr, uval);
	return len;
}

static long long callcache_deserialize_read_int(unsigned char **ptr) {
	return (long long)callcache_deserialize_read_uint(ptr);
}

static size_t callcache_serialize_write_string(unsigned char **ptr, const char *str) {
	size_t len = callcache_serialize_write_uint_raw(ptr, CALLCACHE_CALL_ARG_TYPE_STRING);
	size_t len_data = strlen(str) + 1;
	len += len_data;
	if (ptr) {
		char *serialize_ptr = (char *)*ptr;
		strcpy(serialize_ptr, str);
		serialize_ptr += len_data;
		*ptr = (unsigned char *)serialize_ptr;
	}
	return len;
}

static const char *callcache_deserialize_read_string(unsigned char **ptr) {
	unsigned char *deserialize_ptr = *ptr;
	const char *str = (const char *)deserialize_ptr;
	deserialize_ptr += strlen(str) + 1;
	*ptr = deserialize_ptr;
	return str;
}

static size_t callcache_serialize_write_blob(unsigned char **ptr, const void *data, size_t data_len) {
	size_t len = callcache_serialize_write_uint_raw(ptr, CALLCACHE_CALL_ARG_TYPE_BLOB);
	len += callcache_serialize_write_uint_raw(ptr, data_len);
	len += data_len;
	if (ptr) {
		unsigned char *serialize_ptr = *ptr;
		memcpy(serialize_ptr, data, data_len);
		serialize_ptr += data_len;
		*ptr = serialize_ptr;
	}
	return len;
}

static const void *callcache_deserialize_read_blob(unsigned char **ptr, size_t *len) {
	size_t blob_len = callcache_deserialize_read_uint(ptr);
	unsigned char *deserialize_ptr = *ptr;
	void *data = deserialize_ptr;
	deserialize_ptr += blob_len;
	*ptr = deserialize_ptr;
	*len = blob_len;
	return data;
}

static size_t callcache_serialize_write_ptr(unsigned char **ptr, void *rawptr) {
	size_t len = callcache_serialize_write_uint_raw(ptr, CALLCACHE_CALL_ARG_TYPE_PTR);
	len += callcache_serialize_write_uint_raw(ptr, (uintptr_t)rawptr);
	return len;
}

static void *callcache_deserialize_read_ptr(unsigned char **ptr) {
	return (void *)(uintptr_t)callcache_deserialize_read_uint(ptr);
}

static size_t callcache_serialize_arg_tiny(callcache_call_arg_t *call_arg, unsigned int arg_index, unsigned char **ptr) {
	size_t len = callcache_serialize_write_uint_raw(ptr, arg_index);
	switch(call_arg->type) {
	case CALLCACHE_CALL_ARG_TYPE_INT:
		len += callcache_serialize_write_int(ptr, call_arg->value.int_val);
		break;
	case CALLCACHE_CALL_ARG_TYPE_UINT:
		len += callcache_serialize_write_uint(ptr, call_arg->value.uint_val);
		break;
	case CALLCACHE_CALL_ARG_TYPE_STRING:
		len += callcache_serialize_write_string(ptr, call_arg->value.string_val);
		break;
	case CALLCACHE_CALL_ARG_TYPE_PTR:
		len += callcache_serialize_write_ptr(ptr, call_arg->value.ptr_val);
		break;
	case CALLCACHE_CALL_ARG_TYPE_BLOB:
		len += callcache_serialize_write_blob(ptr, call_arg->value.blob_val.data, call_arg->value.blob_val.len);
		break;
	}

	return len;
}

static size_t callcache_serialize_tiny(const callcache_callee_t *callee, callcache_call_arg_t *call_args, unsigned int num_call_args, unsigned char **ptr) {
	/* Dual pass serialization */
	/* Count number of args to serialize first */
	unsigned int num_cacheable_args = 0;
	if (callee->is_call_arg_cacheable) {
		for (unsigned int arg = 0; arg < num_call_args; arg++) {
			callcache_call_arg_t *call_arg = &call_args[arg];
			if (callee->is_call_arg_cacheable(call_arg, arg)) {
				num_cacheable_args++;
			}
		}
	} else {
		num_cacheable_args = num_call_args;
	}

	/* Write number of arguments */
	size_t len = callcache_serialize_write_uint_raw(ptr, num_cacheable_args);
	/* Write actual arguments */
	for (unsigned int arg = 0; arg < num_call_args; arg++) {
		callcache_call_arg_t *call_arg = &call_args[arg];
		if (callee->is_call_arg_cacheable && !callee->is_call_arg_cacheable(call_arg, arg)) {
			continue;
		}
		len += callcache_serialize_arg_tiny(call_arg, arg, ptr);
	}

	return len;
}

unsigned int callcache_deserialize_get_num_args(void *data) {
	unsigned char *ptr = data;
	return callcache_deserialize_read_uint(&ptr);
}

typedef struct callcache_deserialize_iterator {
	unsigned int num_args;
	unsigned int arg_index;
	unsigned char *ptr;
} callcache_deserialize_iterator_t;

void callcache_deserialize_iterator_get(callcache_deserialize_iterator_t *iter, callcache_call_arg_t *call_arg) {
	call_arg->pos = callcache_deserialize_read_uint(&iter->ptr);
	call_arg->type = callcache_deserialize_read_uint(&iter->ptr);
	switch (call_arg->type) {
	case CALLCACHE_CALL_ARG_TYPE_INT:
		call_arg->value.int_val = callcache_deserialize_read_int(&iter->ptr);
		break;
	case CALLCACHE_CALL_ARG_TYPE_UINT:
		call_arg->value.uint_val = callcache_deserialize_read_uint(&iter->ptr);
		break;
	case CALLCACHE_CALL_ARG_TYPE_STRING:
		call_arg->value.string_val = callcache_deserialize_read_string(&iter->ptr);
		break;
	case CALLCACHE_CALL_ARG_TYPE_PTR:
		call_arg->value.ptr_val = callcache_deserialize_read_ptr(&iter->ptr);
		break;
	case CALLCACHE_CALL_ARG_TYPE_BLOB:
		call_arg->value.blob_val.data = callcache_deserialize_read_blob(&iter->ptr, &call_arg->value.blob_val.len);
		break;
	}
}

bool callcache_deserialize_iterator_valid(callcache_deserialize_iterator_t *iter) {
	return iter->arg_index < iter->num_args;
}

void callcache_deserialize_iterator_first(callcache_deserialize_iterator_t *iter, callcache_call_arg_t *call_arg, void *buf) {
	unsigned char *ptr = buf;
	iter->num_args = callcache_deserialize_read_uint(&ptr);
	iter->arg_index = 0;
	iter->ptr = ptr;
	if (callcache_deserialize_iterator_valid(iter)) {
		callcache_deserialize_iterator_get(iter, call_arg);
	}
}

void callcache_deserialize_iterator_next(callcache_deserialize_iterator_t *iter, callcache_call_arg_t *call_arg) {
	iter->arg_index++;
	if (callcache_deserialize_iterator_valid(iter)) {
		callcache_deserialize_iterator_get(iter, call_arg);
	}
}

static bool compare_arg(callcache_call_arg_t *arg_a, callcache_call_arg_t *arg_b) {
	if (arg_a->type != arg_b->type) {
		return false;
	}

	switch (arg_a->type) {
	case CALLCACHE_CALL_ARG_TYPE_INT:
		return arg_a->value.int_val == arg_b->value.int_val;
	case CALLCACHE_CALL_ARG_TYPE_UINT:
		return arg_a->value.uint_val == arg_b->value.uint_val;
	case CALLCACHE_CALL_ARG_TYPE_PTR:
		return arg_a->value.ptr_val == arg_b->value.ptr_val;
	case CALLCACHE_CALL_ARG_TYPE_STRING:
		return !strcmp(arg_a->value.string_val, arg_b->value.string_val);
	case CALLCACHE_CALL_ARG_TYPE_BLOB:
		if (arg_a->value.blob_val.len != arg_b->value.blob_val.len) {
			return false;
		}
		return !memcmp(arg_a->value.blob_val.data, arg_b->value.blob_val.data, arg_a->value.blob_val.len);
	}

	return false;
}

static bool compare_args(callcache_entry_t *entry, callcache_call_arg_t *args, unsigned int num_args) {
	const callcache_callee_t *callee = entry->callee;
	if (callee->is_call_arg_cacheable) {
		unsigned int num_cacheable_args = 0;
		for (unsigned int arg = 0; arg < num_args; arg++) {
			callcache_call_arg_t *call_arg = &args[arg];
			if (callee->is_call_arg_cacheable(call_arg, arg)) {
				num_cacheable_args++;
			}
		}
		if (num_args != num_cacheable_args) {
			return false;
		}
	}

	callcache_deserialize_iterator_t iter;
	callcache_call_arg_t deserialized_return_value;
	callcache_deserialize_iterator_first(&iter, &deserialized_return_value, entry->serialized_call);
	for (unsigned int arg = 0; arg < num_args; arg++) {
		callcache_call_arg_t *call_arg = &args[arg];
		if (callee->is_call_arg_cacheable && !callee->is_call_arg_cacheable(call_arg, arg)) {
			continue;
		}
		if (!callcache_deserialize_iterator_valid(&iter)) {
			return false;
		}
		if (deserialized_return_value.pos != arg) {
			return false;
		}
		if (!compare_arg(call_arg, &deserialized_return_value)) {
			return false;
		}
		callcache_deserialize_iterator_next(&iter, &deserialized_return_value);
	}

	return true;
}

static bool compare_return_values(callcache_entry_t *entry, callcache_call_arg_t *return_values, unsigned int num_return_values) {
	/* Skip over call args */
	callcache_deserialize_iterator_t iter;
	callcache_call_arg_t call_arg;
	for (callcache_deserialize_iterator_first(&iter, &call_arg, entry->serialized_call);
	     callcache_deserialize_iterator_valid(&iter);
	     callcache_deserialize_iterator_next(&iter, &call_arg));

	/* Compare passed return values to cached return values */
	callcache_call_arg_t deserialized_return_value;
	callcache_deserialize_iterator_first(&iter, &deserialized_return_value, iter.ptr);
	for (unsigned int return_value_idx = 0; return_value_idx < num_return_values; return_value_idx++) {
		callcache_call_arg_t *return_value = &return_values[return_value_idx];
		if (!callcache_deserialize_iterator_valid(&iter)) {
			return false;
		}
		if (!compare_arg(return_value, &deserialized_return_value)) {
			return false;
		}
		callcache_deserialize_iterator_next(&iter, &deserialized_return_value);
	}

	return true;
}

static callcache_entry_t *find_cache_entry(callcache_t *callcache, const callcache_callee_t *callee, callcache_call_arg_t *args, unsigned int num_args) {
	callcache_entry_t *entry;
	LIST_FOR_EACH_ENTRY(entry, &callcache->entries, list) {
		if (entry->callee != callee) {
			continue;
		}
		if (!compare_args(entry, args, num_args)) {
			continue;
		}
		return entry;
	}

	return NULL;
}

static void cache_entry_populate_return_values(callcache_entry_t *entry, callcache_call_arg_t *return_values, unsigned int *num_return_val) {
	if (!num_return_val) {
		return;
	}

	/* Skip over call args */
	callcache_deserialize_iterator_t iter;
	callcache_call_arg_t call_arg;
	for (callcache_deserialize_iterator_first(&iter, &call_arg, entry->serialized_call);
	     callcache_deserialize_iterator_valid(&iter);
	     callcache_deserialize_iterator_next(&iter, &call_arg));

	/* Get return values */
	unsigned int num_populated_return_values = 0;
	callcache_call_arg_t return_value;
	callcache_deserialize_iterator_first(&iter, &return_value, iter.ptr);
	for (unsigned int i = *num_return_val; i > 0; i--) {
		if (!callcache_deserialize_iterator_valid(&iter)) {
			break;
		}
		/*
		 * For string and blob types this passes a reference to our internally
		 * managed memory to the caller. This is a bad idea and will cause problems
		 * once blob and string types are actually used.
		 */
		return_values[num_populated_return_values++] = return_value;
		callcache_deserialize_iterator_next(&iter, &return_value);
	}

	*num_return_val = num_populated_return_values;
}

static esp_err_t cache_update_entry(callcache_t *callcache, const callcache_callee_t *callee, callcache_entry_t *entry,
				    callcache_call_arg_t *args, unsigned int num_args,
				    callcache_call_arg_t *return_values, unsigned int num_return_val,
				    int64_t ttl_us) {
	size_t serialized_len = 0;
	serialized_len += callcache_serialize_tiny(callee, args, num_args, NULL);
	serialized_len += callcache_serialize_tiny(callee, return_values, num_return_val, NULL);

	if (entry) {
		LIST_DELETE(&entry->list);
		if (entry->buffer_len < serialized_len) {
			free(entry);
			entry = NULL;
		}
	}
	if (!entry) {
		entry = malloc(sizeof(callcache_entry_t) + serialized_len);
		if (!entry) {
			ESP_LOGE(TAG, "Failed to allocate cache entry, out of memory");
			return ESP_ERR_NO_MEM;
		}
	}
	INIT_LIST_HEAD(entry->list);
	entry->callee = callee;
	entry->ttl_us = ttl_us;
	entry->buffer_len = serialized_len;
	unsigned char *serialize_ptr = entry->serialized_call;
	callcache_serialize_tiny(callee, args, num_args, &serialize_ptr);
	callcache_serialize_tiny(callee, return_values, num_return_val, &serialize_ptr);

	LIST_APPEND(&entry->list, &callcache->entries);
	return ESP_OK;
}

void callcache_init(callcache_t *callcache) {
	INIT_LIST_HEAD(callcache->entries);
	callcache->lock = xSemaphoreCreateRecursiveMutexStatic(&callcache->lock_buffer);
}

esp_err_t callcache_call_changed(callcache_t *callcache, const callcache_callee_t *callee,
				 callcache_call_arg_t *args, unsigned int num_args,
				 callcache_call_arg_t *return_values, unsigned int *num_return_val,
				 bool *changed) {
	int64_t uptime_us = esp_timer_get_time();
	xSemaphoreTakeRecursive(callcache->lock, portMAX_DELAY);
	callcache_entry_t *cache_entry = find_cache_entry(callcache, callee, args, num_args);
	if (cache_entry && uptime_us <= cache_entry->ttl_us) {
		cache_entry_populate_return_values(cache_entry, return_values, num_return_val);
		*changed = false;
		xSemaphoreGiveRecursive(callcache->lock);
		return ESP_OK;
	}

	int64_t cache_ttl_us = 0;
	esp_err_t err = callee->call(args, num_args, return_values, num_return_val, &cache_ttl_us);
	if (err) {
		return err;
	}
	if (changed) {
		if (cache_entry) {
			*changed = !compare_return_values(cache_entry, return_values, *num_return_val);
		} else {
			*changed = true;
		}
	}

	if (cache_ttl_us > 0) {
		return cache_update_entry(callcache, callee, cache_entry,
					  args, num_args,
					  return_values, *num_return_val,
					  uptime_us + cache_ttl_us);
	}
	xSemaphoreGiveRecursive(callcache->lock);

	return ESP_OK;
}

esp_err_t callcache_call(callcache_t *callcache, const callcache_callee_t *callee,
			 callcache_call_arg_t *args, unsigned int num_args,
			 callcache_call_arg_t *return_values, unsigned int *num_return_val) {
	return callcache_call_changed(callcache, callee, args, num_args, return_values, num_return_val, NULL);
}
