#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "list.h"

typedef enum {
	CALLCACHE_CALL_ARG_TYPE_INT,
	CALLCACHE_CALL_ARG_TYPE_UINT,
	CALLCACHE_CALL_ARG_TYPE_STRING,
	CALLCACHE_CALL_ARG_TYPE_PTR,
	CALLCACHE_CALL_ARG_TYPE_BLOB
} callcache_call_arg_type_t;

typedef struct callcache_call_arg {
	unsigned int pos;
	callcache_call_arg_type_t type;
	union {
		int int_val;
		unsigned int uint_val;
		const char *string_val;
		void *ptr_val;
		struct {
			const void *data;
			size_t len;
		} blob_val;
	} value;
} callcache_call_arg_t;

typedef struct callcache_callee {
	esp_err_t (*call)(callcache_call_arg_t *args, unsigned int num_args, callcache_call_arg_t *return_values, unsigned int *num_return_val, int64_t *cache_ttl_us);
	bool (*is_call_arg_cacheable)(callcache_call_arg_t *arg, unsigned int pos);
} callcache_callee_t;

typedef struct callcache_entry {
	list_head_t list;
	const callcache_callee_t *callee;
	int64_t ttl_us;
	size_t buffer_len;
	unsigned char serialized_call[0];
} callcache_entry_t;

typedef struct callcache {
	list_head_t entries;
	StaticSemaphore_t lock_buffer;
	SemaphoreHandle_t lock;
} callcache_t;

void callcache_init(callcache_t *callcache);
esp_err_t callcache_call(callcache_t *callcache, const callcache_callee_t *callee, callcache_call_arg_t *args, unsigned int num_args, callcache_call_arg_t *return_values, unsigned int *num_return_val);
esp_err_t callcache_call_changed(callcache_t *callcache, const callcache_callee_t *callee, callcache_call_arg_t *args, unsigned int num_args, callcache_call_arg_t *return_values, unsigned int *num_return_val, bool *value_changed);
